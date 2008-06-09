/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
 * Copyright (C) 2006 Nokia Corporation. All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <string.h>
#include <sys/types.h>

/* Needed on Mac OS X */
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

/* Needed on Mac OS X */
#if HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif

#include <arpa/nameser.h>
#include <resolv.h>

#include "lm-debug.h"
#include "lm-internals.h"
#include "lm-misc.h"
#include "lm-ssl.h"
#include "lm-ssl-internals.h"
#include "lm-proxy.h"
#include "lm-socket.h"
#include "lm-sock.h"
#include "lm-error.h"

#ifdef HAVE_ASYNCNS
#include <asyncns.h>
#define freeaddrinfo(x) asyncns_freeaddrinfo(x)
#endif

#define IN_BUFFER_SIZE 1024
#define MIN_PORT 1
#define MAX_PORT 65536
#define SRV_LEN 8192

struct _LmSocket {
	LmConnection *connection;
	GMainContext *context;

	gchar        *domain;
	gchar        *server;
	guint         port;

	gboolean      blocking;

	LmSSL        *ssl;
	gboolean      ssl_started;
	gboolean      ssl_starting;
	LmProxy      *proxy;

	GIOChannel   *io_channel;
	GSource      *watch_in;
	GSource      *watch_err;
	GSource      *watch_hup;

	LmSocketT      fd;

	GSource      *watch_connect;

	gboolean      cancel_open;
	
	GSource      *watch_out;
	GString      *out_buf;

	LmConnectData *connect_data;

	IncomingDataFunc data_func;
	SocketClosedFunc closed_func;
	ConnectResultFunc connect_func;
	gpointer         user_data;

	guint          ref_count;

#ifdef HAVE_ASYNCNS
	GSource		*watch_resolv;
	asyncns_query_t *resolv_query;
	asyncns_t	*asyncns_ctx;
	GIOChannel	*resolv_channel;
#endif
}; 

struct _LmSocketTls {
  LmSocket *socket;
  void (*tls_success)(gpointer data);
  void (*tls_failed)(gpointer data);
};

static void         socket_free               (LmSocket       *socket);
static gboolean     socket_do_connect         (LmConnectData  *connect_data);
static gboolean     socket_connect_cb         (GIOChannel     *source, 
					       GIOCondition    condition,
					       LmConnectData  *connect_data);
static gboolean     socket_in_event           (GIOChannel     *source,
					       GIOCondition    condition,
					       LmSocket       *socket);
static gboolean     socket_hup_event          (GIOChannel     *source,
					       GIOCondition    condition,
					       LmSocket       *socket);
static gboolean     socket_error_event        (GIOChannel     *source,
					       GIOCondition    condition,
					       LmSocket       *socket);
static gboolean     socket_buffered_write_cb  (GIOChannel     *source, 
					       GIOCondition    condition,
					       LmSocket       *socket);
static gboolean     socket_parse_srv_response (unsigned char  *srv, 
					       int             srv_len, 
					       gchar         **out_server, 
					       guint          *out_port);
static void         socket_close_io_channel   (GIOChannel     *io_channel);
static void         _lm_socket_succeeded (LmSocket *socket);

static void
socket_free (LmSocket *socket)
{
	g_free (socket->server);
	g_free (socket->domain);

	if (socket->ssl) {
		lm_ssl_unref (socket->ssl);
	}

	if (socket->proxy) {
		lm_proxy_unref (socket->proxy);
	}
	
	if (socket->out_buf) {
		g_string_free (socket->out_buf, TRUE);
	}

	g_free (socket);
}

gint
lm_socket_do_write (LmSocket *socket, const gchar *buf, gint len)
{
	gint b_written;

	if (socket->ssl_started) {
		b_written = _lm_ssl_send (socket->ssl, buf, len);
	} else {
		GIOStatus io_status = G_IO_STATUS_AGAIN;
		gsize     bytes_written;

		while (io_status == G_IO_STATUS_AGAIN) {
			io_status = g_io_channel_write_chars (socket->io_channel, 
							      buf, len, 
							      &bytes_written,
							      NULL);
		}

		b_written = bytes_written;

		if (io_status != G_IO_STATUS_NORMAL) {
			b_written = -1;
		}
	}

	return b_written;
}

static gboolean
socket_read_incoming (LmSocket *socket,
		      gchar    *buf,
		      gsize     buf_size,
		      gsize    *bytes_read,
		      gboolean *hangup,
		      gint     *reason)
{
	GIOStatus status;

	*hangup = FALSE;

	if (socket->ssl_started) {
		status = _lm_ssl_read (socket->ssl, 
				       buf, buf_size - 1, bytes_read);
	} else {
		status = g_io_channel_read_chars (socket->io_channel,
						  buf, buf_size - 1,
						  bytes_read,
						  NULL);
	}

	if (status != G_IO_STATUS_NORMAL || *bytes_read < 0) {
		switch (status) {
		case G_IO_STATUS_EOF:
			*reason = LM_DISCONNECT_REASON_HUP;
			break;
		case G_IO_STATUS_AGAIN:
			/* No data readable but we didn't hangup */
			return FALSE;
			break;
		case G_IO_STATUS_ERROR:
			*reason = LM_DISCONNECT_REASON_ERROR;
			break;
		default:
			*reason = LM_DISCONNECT_REASON_UNKNOWN;
		}

		/* Notify connection_in_event that we hangup the connection */
		*hangup = TRUE;
		
		return FALSE;
	}

	buf[*bytes_read] = '\0';

	/* There is more data to be read */
	return TRUE;
}

static gboolean
socket_in_event (GIOChannel   *source,
		     GIOCondition  condition,
		     LmSocket     *socket)
{
	gchar     buf[IN_BUFFER_SIZE];
	gsize     bytes_read = 0;
	gboolean  read_anything = FALSE;
	gboolean  hangup = 0;
	gint      reason = 0;

	if (!socket->io_channel) {
		return FALSE;
	}

	while ((condition & G_IO_IN) && socket_read_incoming (socket, buf, IN_BUFFER_SIZE, 
				     &bytes_read, &hangup, &reason)) {
		
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "\nRECV [%d]:\n", 
		       (int)bytes_read);
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
		       "-----------------------------------\n");
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "'%s'\n", buf);
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
		       "-----------------------------------\n");
		
		lm_verbose ("Read: %d chars\n", (int)bytes_read);

		(socket->data_func) (socket, buf, socket->user_data);

		read_anything = TRUE;

		condition = g_io_channel_get_buffer_condition (socket->io_channel);
	}

	/* If we have read something, delay the hangup so that the data can be
	 * processed. */
	if (hangup && !read_anything) {
		(socket->closed_func) (socket, reason, socket->user_data);
		return FALSE;
	}

	return TRUE;
}
	
static gboolean
socket_hup_event (GIOChannel   *source,
		      GIOCondition  condition,
		      LmSocket     *socket)
{
	lm_verbose ("HUP event: %d->'%s'\n", 
		    condition, lm_misc_io_condition_to_str (condition));

	if (!socket->io_channel) {
		return FALSE;
	}

	(socket->closed_func) (socket, LM_DISCONNECT_REASON_HUP, 
			       socket->user_data);
	
	return TRUE;
}

static gboolean
socket_error_event (GIOChannel   *source,
		    GIOCondition  condition,
		    LmSocket     *socket)
{
	lm_verbose ("ERROR event: %d->'%s'\n", 
		    condition, lm_misc_io_condition_to_str (condition));

	if (!socket->io_channel) {
		return FALSE;
	}

	(socket->closed_func) (socket, LM_DISCONNECT_REASON_ERROR, 
			       socket->user_data);
	
	return TRUE;
}

static LmSslErrorCode
_lm_socket_ssl_init (LmSocket *socket, gboolean delayed)
{
	GError *error = NULL;
	const gchar *ssl_verify_domain = NULL;
  int ret;

	lm_verbose ("Setting up SSL...\n");

  if (! socket->ssl_started)
    {
	    _lm_ssl_initialize (socket->ssl);
	    socket->ssl_started = TRUE;
    }

	/* If we're using StartTLS, the correct thing is to verify against
	 * the domain. If we're using old SSL, we should verify against the
	 * hostname. */
	if (delayed)
		ssl_verify_domain = socket->domain;
	else
		ssl_verify_domain = socket->server;
	
	ret = _lm_ssl_begin (socket->ssl, socket->fd, ssl_verify_domain, &error);

	lm_verbose ("_lm_ssl_begin returned %d \n", ret);

  /* do not close the connection */
  if (ret == LM_SSL_EAGAIN_READ || ret == LM_SSL_EAGAIN_WRITE)
    return ret;

	if (ret != LM_SSL_SUCCESS) {
		lm_verbose ("Could not begin SSL\n");

		if (error) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
				"%s\n", error->message);
				g_error_free (error);
		}

		_lm_sock_shutdown (socket->fd);
		_lm_sock_close (socket->fd);

		if (!delayed && socket->connect_func) {
			(socket->connect_func) (socket, FALSE, socket->user_data);
		}
		
		return ret;
	}

	socket->ssl_started = TRUE;

  return 0;
}

LmSslErrorCode
lm_socket_starttls (LmSocket *socket)
{
	g_return_val_if_fail (lm_ssl_get_use_starttls (socket->ssl) == TRUE, FALSE);

	return _lm_socket_ssl_init (socket, TRUE);
}

typedef struct {
  LmSocket *socket;
  void (*tls_success)(gpointer data);
  void (*tls_failed)(gpointer data);
} tls_continue_t;

static gboolean
_tls_continue (GIOChannel *source,
    GIOCondition condition,
    gpointer data)
{
  int ret;
  LmSocket *socket = ((tls_continue_t*)data)->socket;

  ret = lm_socket_starttls (socket);

  switch (ret)
    {
      case LM_SSL_EAGAIN_READ:
        socket->watch_in = 
          lm_misc_add_io_watch (socket->context,
                    socket->io_channel,
                    G_IO_IN,
                    (GIOFunc) _tls_continue,
                    data);
        break;

      case LM_SSL_EAGAIN_WRITE:
        socket->watch_out = 
          lm_misc_add_io_watch (socket->context,
                    socket->io_channel,
                    G_IO_OUT,
                    (GIOFunc) _tls_continue,
                    data);
        break;

      case LM_SSL_SUCCESS:
        socket->watch_in = 
          lm_misc_add_io_watch (socket->context,
                    socket->io_channel,
                    G_IO_IN,
                    (GIOFunc) socket_in_event,
                    socket);
        ((tls_continue_t*)data)->tls_success(socket->connection);
        g_slice_free (tls_continue_t, data);
        break;
      
      case LM_SSL_FAILURE:
        ((tls_continue_t*)data)->tls_failed(socket->connection);
        g_slice_free (tls_continue_t, data);
        break;
    }

  return FALSE;
}

void
lm_socket_start_tls (LmSocket *socket,
    void (tls_success)(gpointer data),
    void (tls_failed)(gpointer data))
{
  tls_continue_t *data;

  data = g_slice_new0 (tls_continue_t);
  data->socket = socket;
  data->tls_success = tls_success;
  data->tls_failed = tls_failed;

  g_assert (g_source_remove (g_source_get_id (socket->watch_in)));

  _tls_continue (NULL, 0, data);
}

static gboolean
_oldssl_continue(GIOChannel *source,
    GIOCondition condition,
    gpointer data)
{
  int ret;
	LmSocket *socket = data;

  ret = _lm_socket_ssl_init (socket, FALSE);
  switch (ret)
    {
      case LM_SSL_FAILURE:
        break; /* FIXME: do something useful? */

      case LM_SSL_SUCCESS:
        _lm_socket_succeeded (socket);
        break;

      case LM_SSL_EAGAIN_WRITE:
        socket->watch_out = 
          lm_misc_add_io_watch (socket->context,
                    socket->io_channel,
                    G_IO_OUT,
                    (GIOFunc) _oldssl_continue,
                    socket);
        break;

      case LM_SSL_EAGAIN_READ:
        socket->watch_in = 
          lm_misc_add_io_watch (socket->context,
                    socket->io_channel,
                    G_IO_IN,
                    (GIOFunc) _oldssl_continue,
                    socket);
        break;

    }
  return FALSE;
}

void
_lm_socket_connect_succeeded (LmConnectData *connect_data)
{
	LmSocket     *socket;
	
	socket = connect_data->socket;

	if (socket->watch_connect) {
		g_source_destroy (socket->watch_connect);
		socket->watch_connect = NULL;
	}

	/* Need some way to report error/success */
	if (socket->cancel_open) {
		lm_verbose ("Cancelling connection...\n");
		if (socket->connect_func) {
			(socket->connect_func) (socket, FALSE, socket->user_data);
		}
		return;
	}
	
	socket->fd = connect_data->fd;
	socket->io_channel = connect_data->io_channel;

	freeaddrinfo (connect_data->resolved_addrs);
	socket->connect_data = NULL;
	g_free (connect_data);

	/* old-style ssl should be started immediately */
	if (socket->ssl && (lm_ssl_get_use_starttls (socket->ssl) == FALSE)) {
    _oldssl_continue (NULL, 0, socket);
    return;
	}

  _lm_socket_succeeded (socket);
}

static void
_lm_socket_succeeded (LmSocket *socket)
{
	socket->watch_in = 
		lm_misc_add_io_watch (socket->context,
				      socket->io_channel,
				      G_IO_IN,
				      (GIOFunc) socket_in_event,
				      socket);

	/* FIXME: if we add these, we don't get ANY
	 * response from the server, this is to do with the way that
	 * windows handles watches, see bug #331214.
	 */
#ifndef G_OS_WIN32
	socket->watch_err = 
		lm_misc_add_io_watch (socket->context,
				      socket->io_channel,
				      G_IO_ERR,
				      (GIOFunc) socket_error_event,
				      socket);
		
	socket->watch_hup =
		lm_misc_add_io_watch (socket->context,
				      socket->io_channel,
				      G_IO_HUP,
				      (GIOFunc) socket_hup_event,
				      socket);
#endif

	if (socket->connect_func) {
		(socket->connect_func) (socket, TRUE, socket->user_data);
	}
}

gboolean 
_lm_socket_failed_with_error (LmConnectData *connect_data, int error) 
{
	LmSocket *socket;
	
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Connection failed: %s (error %d)\n",
	       _lm_sock_get_error_str (error), error);
	
	socket = lm_socket_ref (connect_data->socket);

	connect_data->current_addr = connect_data->current_addr->ai_next;
	
	if (socket->watch_connect) {
		g_source_destroy (socket->watch_connect);
		socket->watch_connect = NULL;
	}

	if (connect_data->io_channel != NULL) {
		socket_close_io_channel (connect_data->io_channel);
	}
	
	if (connect_data->current_addr == NULL) {
		if (socket->connect_func) {
			(socket->connect_func) (socket, FALSE, socket->user_data);
		}
		
		 /* if the user callback called connection_close(), this is already freed */
		if (socket->connect_data != NULL) {
			freeaddrinfo (connect_data->resolved_addrs);
			socket->connect_data = NULL;
			g_free (connect_data);
		}
	} else {
		/* try to connect to the next host */
		return socket_do_connect (connect_data);
	}

	lm_socket_unref(socket);

	return FALSE;
}

gboolean 
_lm_socket_failed (LmConnectData *connect_data)
{
	return _lm_socket_failed_with_error (connect_data,
                                       _lm_sock_get_last_error());
}

static gboolean 
socket_connect_cb (GIOChannel   *source, 
		       GIOCondition  condition,
		       LmConnectData *connect_data) 
{
	LmSocket        *socket;
	struct addrinfo *addr;
	int              err;
	socklen_t        len;
	LmSocketT        fd;
	gboolean         result = FALSE;

	socket = lm_socket_ref (connect_data->socket);
	addr = connect_data->current_addr;
	fd = g_io_channel_unix_get_fd (source);

	if (condition == G_IO_ERR) {
		len = sizeof (err);
		_lm_sock_get_error (fd, &err, &len);
		if (!_lm_sock_is_blocking_error (err)) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
			       "Connection failed.\n");

			/* error condition, but might be possible to recover
			 * from it (by connecting to the next host) */
			if (!_lm_socket_failed_with_error (connect_data, err)) {
				socket->watch_connect = NULL;
				goto out;
			}
		}
	}

	if (_lm_connection_async_connect_waiting (socket->connection)) {
		gint res;

		fd = g_io_channel_unix_get_fd (source);

		res = _lm_sock_connect (fd, addr->ai_addr, (int)addr->ai_addrlen);  
		if (res < 0) {
			err = _lm_sock_get_last_error ();
			if (_lm_sock_is_blocking_success (err)) {
				_lm_connection_set_async_connect_waiting (socket->connection, FALSE);

				g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
				       "Connection success (1).\n");
				
				_lm_socket_connect_succeeded (connect_data);
			}
			
			if (_lm_connection_async_connect_waiting (socket->connection) &&
			    !_lm_sock_is_blocking_error (err)) {
				g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
				       "Connection failed.\n");

				_lm_sock_close (connect_data->fd);
				_lm_socket_failed_with_error (connect_data, err);

				socket->watch_connect = NULL;
				goto out;
			}
		} 
	} else {		
		/* for blocking sockets, G_IO_OUT means we are connected */
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Connection success (2).\n");
		
		_lm_socket_connect_succeeded (connect_data);
	}

	result = TRUE;

 out:
	lm_socket_unref(socket);
	
 	return result; 
}

static gboolean
socket_do_connect (LmConnectData *connect_data) 
{
	LmSocket        *socket;
	LmSocketT        fd;
	int              res, err;
	int              port;
	char             name[NI_MAXHOST];
	char             portname[NI_MAXSERV];
	struct addrinfo *addr;
	
	socket = connect_data->socket;
	addr = connect_data->current_addr;
 
	if (socket->proxy) {
		port = htons (lm_proxy_get_port (socket->proxy));
	} else {
		port = htons (socket->port);
	}
	
	((struct sockaddr_in *) addr->ai_addr)->sin_port = port;

	res = getnameinfo (addr->ai_addr,
			   (socklen_t)addr->ai_addrlen,
			   name,     sizeof (name),
			   portname, sizeof (portname),
			   NI_NUMERICHOST | NI_NUMERICSERV);
	
	if (res < 0) {
		return _lm_socket_failed (connect_data);
	}

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Trying %s port %s...\n", name, portname);
	
	fd = _lm_sock_makesocket (addr->ai_family,
				  addr->ai_socktype, 
				  addr->ai_protocol);
	
	if (!_LM_SOCK_VALID (fd)) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
		       "Failed making socket, error:%d...\n",
		       _lm_sock_get_last_error ());

		return _lm_socket_failed (connect_data);
	}

	/* Even though it says _unix_new(), it is supported by glib on
	 * win32 because glib does some cool stuff to find out if it
	 * can treat it as a FD or a windows SOCKET.
	 */
	connect_data->fd = fd;
	connect_data->io_channel = g_io_channel_unix_new (fd);
	
	g_io_channel_set_encoding (connect_data->io_channel, NULL, NULL);
	g_io_channel_set_buffered (connect_data->io_channel, FALSE);

	_lm_sock_set_blocking (connect_data->fd, socket->blocking);
	
	if (socket->proxy) {
		socket->watch_connect =
			lm_misc_add_io_watch (socket->context,
					      connect_data->io_channel,
					      G_IO_OUT|G_IO_ERR,
					      (GIOFunc) _lm_proxy_connect_cb, 
					      connect_data);
	} else {
		socket->watch_connect =
			lm_misc_add_io_watch (socket->context,
					      connect_data->io_channel,
					      G_IO_OUT|G_IO_ERR,
					      (GIOFunc) socket_connect_cb,
					      connect_data);
	}

	_lm_connection_set_async_connect_waiting (socket->connection, !socket->blocking);

  	res = _lm_sock_connect (connect_data->fd, 
				addr->ai_addr, (int)addr->ai_addrlen);  
	if (res < 0) {
		err = _lm_sock_get_last_error ();
		if (!_lm_sock_is_blocking_error (err)) {
			_lm_sock_close (connect_data->fd);
			return _lm_socket_failed_with_error (connect_data, err);
		}
	}

	return TRUE;
}

gboolean
lm_socket_output_is_buffered (LmSocket     *socket,
			       const gchar  *buffer,
			       gint          len)
{
	if (socket->out_buf) {
		lm_verbose ("Appending %d bytes to output buffer\n", len);
		g_string_append_len (socket->out_buf, buffer, len);
		return TRUE;
	}

	return FALSE;
}

void
lm_socket_setup_output_buffer (LmSocket *socket, const gchar *buffer, gint len)
{
	lm_verbose ("OUTPUT BUFFER ENABLED\n");

	socket->out_buf = g_string_new_len (buffer, len);

	socket->watch_out =
		lm_misc_add_io_watch (socket->context,
				      socket->io_channel,
				      G_IO_OUT,
				      (GIOFunc) socket_buffered_write_cb,
				      socket);
}

static gboolean
socket_buffered_write_cb (GIOChannel   *source, 
			  GIOCondition  condition,
			  LmSocket     *socket)
{
	gint     b_written;
	GString *out_buf;
	/* FIXME: Do the writing */

	out_buf = socket->out_buf;
	if (!out_buf) {
		/* Should not be possible */
		return FALSE;
	}

	b_written = lm_socket_do_write (socket, out_buf->str, out_buf->len);

	if (b_written < 0) {
		(socket->closed_func) (socket, LM_DISCONNECT_REASON_ERROR, 
				       socket->user_data);
		return FALSE;
	}

	g_string_erase (out_buf, 0, (gsize) b_written);
	if (out_buf->len == 0) {
		lm_verbose ("Output buffer is empty, going back to normal output\n");

		if (socket->watch_out) {
			g_source_destroy (socket->watch_out);
			socket->watch_out = NULL;
		}

		g_string_free (out_buf, TRUE);
		socket->out_buf = NULL;
		return FALSE;
	}

	return TRUE;
}

static gboolean
socket_parse_srv_response (unsigned char  *srv, 
			   int             srv_len, 
			   gchar         **out_server, 
			   guint          *out_port)
{
	int                  qdcount;
	int                  ancount;
	int                  len;
	const unsigned char *pos;
	unsigned char       *end;
	HEADER              *head;
	char                 name[256];
	char                 pref_name[256];
	guint                pref_port = 0;
	guint                pref_prio = 9999;

	pref_name[0] = 0;

	pos = srv + sizeof (HEADER);
	end = srv + srv_len;
	head = (HEADER *) srv;

	qdcount = ntohs (head->qdcount);
	ancount = ntohs (head->ancount);

	/* Ignore the questions */
	while (qdcount-- > 0 && (len = dn_expand (srv, end, pos, name, 255)) >= 0) {
		g_assert (len >= 0);
		pos += len + QFIXEDSZ;
	}

	/* Parse the answers */
	while (ancount-- > 0 && (len = dn_expand (srv, end, pos, name, 255)) >= 0) {
		/* Ignore the initial string */
		uint16_t pref, weight, port;

		g_assert (len >= 0);
		pos += len;
		/* Ignore type, ttl, class and dlen */
		pos += 10;
		GETSHORT (pref, pos);
		GETSHORT (weight, pos);
		GETSHORT (port, pos);

		len = dn_expand (srv, end, pos, name, 255);
		if (pref < pref_prio) {
			pref_prio = pref;
			strcpy (pref_name, name);
			pref_port = port;
		}
		pos += len;
	}

	if (pref_name[0]) {
		*out_server = g_strdup (pref_name);
		*out_port = pref_port;
		return TRUE;
	} 
	return FALSE;
}

static void
socket_close_io_channel (GIOChannel *io_channel)
{
	gint fd;

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
	       "Freeing up IOChannel and file descriptor\n");

	fd = g_io_channel_unix_get_fd (io_channel);

	g_io_channel_unref (io_channel);

	_lm_sock_close (fd);
}

static void
_lm_socket_create_phase1 (LmSocket *socket, unsigned char *srv_ans, int len);
static void
_lm_socket_create_phase2 (LmSocket *socket, struct addrinfo *ans);

#ifdef HAVE_ASYNCNS
#define PHASE_1 0
#define PHASE_2 1

static gboolean
_lm_socket_resolver_done (GSource *source,
    			  GIOCondition condition,
			  gpointer data);


static void
_asyncns_done (LmSocket *socket)
{
	if (socket->resolv_channel != NULL) {
		g_io_channel_unref (socket->resolv_channel);
		socket->resolv_channel = NULL;
	}
 
	if (socket->watch_resolv) {
		g_source_destroy(socket->watch_resolv);
		socket->watch_resolv = NULL;
	}

	if (socket->asyncns_ctx) {
		asyncns_free (socket->asyncns_ctx);
		socket->asyncns_ctx = NULL;
	}

 	socket->resolv_query = NULL;
}

void _asyncns_cancel (LmSocket *socket)
{
	if (socket == NULL)
		return;

	if (socket->asyncns_ctx) {
		if (socket->resolv_query)
			asyncns_cancel (socket->asyncns_ctx, socket->resolv_query);

		_asyncns_done (socket);
	}
}

static gboolean
_asyncns_prep (LmSocket *socket, GError **error)
{
	if (socket->asyncns_ctx) {
		return TRUE;
	}

	socket->asyncns_ctx = asyncns_new (1);
	if (socket->asyncns_ctx == NULL) {
		g_set_error (error,
				LM_ERROR,                 
				LM_ERROR_CONNECTION_FAILED,   
				"can't initialise libasyncns");
		return FALSE;
	}

	socket->resolv_channel =
		g_io_channel_unix_new (asyncns_fd (socket->asyncns_ctx));

	socket->watch_resolv = 
		lm_misc_add_io_watch (socket->context,
				      socket->resolv_channel,
				      G_IO_IN,
				      (GIOFunc) _lm_socket_resolver_done,
				      socket);

	return TRUE;
}

static gboolean
_lm_socket_resolver_done (GSource *source,
    			  GIOCondition condition,
			  gpointer data)
{
	LmSocket	*socket = lm_socket_ref ((LmSocket *) data);
	struct addrinfo	*ans;
	unsigned char   *srv_ans;
	int 		 err;
	gboolean         result = FALSE;

	/* process pending data */
	asyncns_wait (socket->asyncns_ctx, FALSE);

	if (!asyncns_isdone (socket->asyncns_ctx, socket->resolv_query)) {
		result = TRUE;
	} else {
		switch ((guint) asyncns_getuserdata (socket->asyncns_ctx, socket->resolv_query)) {
		case PHASE_1:
			err = asyncns_res_done (socket->asyncns_ctx, socket->resolv_query, &srv_ans);
			socket->resolv_query = NULL;
			_lm_socket_create_phase1 (socket, (err <= 0) ? NULL : srv_ans, err);
			result = TRUE;
			break;
		case PHASE_2:
			err = asyncns_getaddrinfo_done (socket->asyncns_ctx, socket->resolv_query, &ans);
			socket->resolv_query = NULL;
			_lm_socket_create_phase2 (socket, (err) ? NULL : ans);
			_asyncns_done (socket);
			break;
		default:
			g_assert_not_reached();
			break;
		}
	}

	lm_socket_unref(socket);
	
	return result;
}

#endif

static void
_lm_socket_create_phase1 (LmSocket *socket,
			  unsigned char *srv_ans,
			  int len)
{
	const char          *remote_addr;
	LmConnectData       *data;
	struct addrinfo      req;
#ifndef HAVE_ASYNCNS
	struct addrinfo *ans;
	int              err;
#endif

	if (srv_ans != NULL) {
		gchar    *new_server;
		guint     new_port;
		gboolean  result;
		result = socket_parse_srv_response (srv_ans, len, 
						    &new_server, 
						    &new_port);
		if (result == TRUE) {
			g_free (socket->server);
			socket->server = new_server;
			socket->port = new_port;
		}
	}

	/* If server wasn't specified and SRV failed, use domain */
	if (!socket->server) {
		lm_verbose ("SRV lookup failed, trying jid domain\n");
		socket->server = g_strdup (socket->domain);
	}

	if (socket->proxy) {
		remote_addr = lm_proxy_get_server (socket->proxy);
	} else {
		remote_addr = socket->server;
	}

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Going to connect to %s%s:%u\n", (socket->proxy) ? "proxy " : "",
	       remote_addr, socket->port);

	data = g_new0 (LmConnectData, 1);

	data->socket        = socket;
	data->connection    = socket->connection;
	data->fd            = -1;
	socket->connect_data = data;

	memset (&req, 0, sizeof(req));
	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;

#ifdef HAVE_ASYNCNS
	if (!_asyncns_prep (socket, NULL))
		return;

	socket->resolv_query =
	  	asyncns_getaddrinfo (socket->asyncns_ctx,
		    		     remote_addr,
				     NULL,
				     &req);

	asyncns_setuserdata (socket->asyncns_ctx,
			     socket->resolv_query,
			     (gpointer) PHASE_2);
#else
	err = getaddrinfo (remote_addr, NULL, &req, &ans);
	_lm_socket_create_phase2 (socket, (err) ? NULL : ans);
	if (err != 0) {
		return;
	}
#endif
}

static void
_lm_socket_create_phase2 (LmSocket *socket, struct addrinfo *ans)
{
	if (ans == NULL) {
		lm_verbose ("error while resolving, bailing out\n");
		if (socket->connect_func) {
			(socket->connect_func) (socket, FALSE, socket->user_data);
		}
		g_free (socket->connect_data);
		socket->connect_data = NULL;
		return;
	}

	socket->connect_data->resolved_addrs = ans;
	socket->connect_data->current_addr   = ans;

	socket_do_connect (socket->connect_data);
}

LmSocket *
lm_socket_create (GMainContext      *context,
		  IncomingDataFunc   data_func,
		  SocketClosedFunc   closed_func,
		  ConnectResultFunc  connect_func,
		  gpointer           user_data,
		  LmConnection      *connection,
		  gboolean           blocking,
		  const gchar       *server,
		  const gchar       *domain,
		  guint              port, 
		  LmSSL             *ssl,
		  LmProxy           *proxy,
		  GError           **error)
{
	LmSocket        *socket;

#ifndef HAVE_ASYNCNS
	unsigned char    srv_ans[SRV_LEN];
	int              len;
#endif
	
	g_return_val_if_fail (domain != NULL, NULL);
	g_return_val_if_fail ((port >= MIN_PORT && port <= MAX_PORT), NULL);
	g_return_val_if_fail (data_func != NULL, NULL);
	g_return_val_if_fail (closed_func != NULL, NULL);
	g_return_val_if_fail (connect_func != NULL, NULL);

	socket = g_new0 (LmSocket, 1);

	socket->ref_count = 1;

	socket->connection = connection;
	socket->domain = g_strdup (domain);
	socket->server = g_strdup (server);
	socket->port = port;
	socket->cancel_open = FALSE;
	socket->ssl = ssl;
	socket->ssl_started = FALSE;
	socket->proxy = NULL;
	socket->blocking = blocking;

	if (context) {
		socket->context = g_main_context_ref (context);
	}

	if (proxy) {
		socket->proxy = lm_proxy_ref (proxy);
	}

	if (!server) {
		char          *srv;
		srv = g_strdup_printf ("_xmpp-client._tcp.%s", socket->domain);
		lm_verbose ("Performing a SRV lookup for %s\n", srv);

#ifdef HAVE_ASYNCNS
		if (!_asyncns_prep (socket, error))
			return NULL;
		
		socket->resolv_query =
			asyncns_res_query (socket->asyncns_ctx, srv, C_IN, T_SRV);
		asyncns_setuserdata (socket->asyncns_ctx, socket->resolv_query, (gpointer) PHASE_1);
#else
		res_init ();

		len = res_query (srv, C_IN, T_SRV, srv_ans, SRV_LEN);
		_lm_socket_create_phase1 (socket, (len < 1) ? NULL : srv_ans, len);
		g_free (srv);
#endif
	} else {
		lm_verbose ("SRV lookup disabled for %s\n", socket->server);
		_lm_socket_create_phase1 (socket, NULL, 0);
	}

	if (socket->connect_data == NULL) {
		/* Open failed synchronously, probably a DNS lookup problem */
		lm_socket_unref(socket);
		
		g_set_error (error,
			     LM_ERROR,                 
			     LM_ERROR_CONNECTION_FAILED,   
			     "Failed to resolve server");
		
		return NULL;
	}
		

	/* If the connection fails synchronously, we don't want to call the
	 * connect_func to indicate an error, we return an error indication
	 * instead. So, we delay saving the functions until after we know
	 * we are going to return success.
	 */
	socket->data_func = data_func;
	socket->closed_func = closed_func;
	socket->connect_func = connect_func;
	socket->user_data = user_data;
	
	return socket;
}

void
lm_socket_flush (LmSocket *socket)
{
	g_return_if_fail (socket != NULL);
	g_return_if_fail (socket->io_channel != NULL);

	g_io_channel_flush (socket->io_channel, NULL);
}

void
lm_socket_close (LmSocket *socket)
{
	LmConnectData *data;

	g_return_if_fail (socket != NULL);

	if (socket->watch_connect) {
		g_source_destroy (socket->watch_connect);
		socket->watch_connect = NULL;
	}
	
	data = socket->connect_data;
	if (data) {
		freeaddrinfo (data->resolved_addrs);
		socket->connect_data = NULL;
		g_free (data);
	}

	if (socket->io_channel) {
		if (socket->watch_in) {
			g_source_destroy (socket->watch_in);
			socket->watch_in = NULL;
		}

		if (socket->watch_err) {
			g_source_destroy (socket->watch_err);
			socket->watch_err = NULL;
		}

		if (socket->watch_hup) {
			g_source_destroy (socket->watch_hup);
			socket->watch_hup = NULL;
		}

		if (socket->watch_out) {
			g_source_destroy (socket->watch_out);
			socket->watch_out = NULL;
		}

		socket_close_io_channel (socket->io_channel);

		socket->io_channel = NULL;
		socket->fd = -1;
	}
}

gchar *
lm_socket_get_local_host (LmSocket *socket)
{
	return _lm_sock_get_local_host (socket->fd);
}

LmSocket *
lm_socket_ref (LmSocket *socket)
{
	g_return_val_if_fail (socket != NULL, NULL);

	socket->ref_count++;

	return socket;
}

void
lm_socket_unref (LmSocket *socket)
{
	g_return_if_fail (socket != NULL);

	socket->ref_count--;

	if (socket->ref_count <= 0) {
		socket_free (socket);
	}
}

gboolean
lm_socket_set_keepalive (LmSocket *socket, int delay)
{
#ifdef USE_TCP_KEEPALIVES
	return _lm_sock_set_keepalive (socket->fd, delay);
#else
	return FALSE;
#endif /* USE_TCP_KEEPALIVES */
}

