/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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
#include <sys/stat.h> 
#include <sys/types.h>
#include <fcntl.h>

#include <glib.h>

#include "lm-sock.h"
#include "lm-debug.h"
#include "lm-error.h"
#include "lm-internals.h"
#include "lm-ssl-internals.h"
#include "lm-parser.h"
#include "lm-sha.h"
#include "lm-connection.h"
#include "lm-utils.h"

#define IN_BUFFER_SIZE 1024

typedef struct {
	LmHandlerPriority  priority;
	LmMessageHandler  *handler;
} HandlerData;

typedef struct {
	GSource       source;
	LmConnection *connection;
} LmIncomingSource;

struct _LmConnection {
	/* Parameters */
	GMainContext *context;
	gchar        *server;
	gchar        *jid;
	guint         port;

	LmSSL        *ssl;

	LmProxy      *proxy;
	
	LmParser     *parser;
	gchar        *stream_id;

	GHashTable   *id_handlers;
	GSList       *handlers[LM_MESSAGE_TYPE_UNKNOWN];

	/* Communication */
	GIOChannel   *io_channel;
	guint         io_watch_in;
	guint         io_watch_err;
	guint         io_watch_hup;
	LmSocket      fd;
	guint         io_watch_connect;
	
	guint         open_id;
	LmCallback   *open_cb;

 	gboolean      async_connect_waiting;
	gboolean      blocking;

	gboolean      cancel_open;
	LmCallback   *close_cb;     /* unused */
	LmCallback   *auth_cb;
	LmCallback   *register_cb;  /* unused */

	LmCallback   *disconnect_cb;

	GQueue       *incoming_messages;
	GSource      *incoming_source;

	LmConnectionState state;

	guint         keep_alive_rate;
	guint         keep_alive_id;

	guint         io_watch_out;
	GString      *out_buf;

	LmConnectData *connect_data;

	gint          ref_count;
};

typedef enum {
	AUTH_TYPE_PLAIN  = 1,
	AUTH_TYPE_DIGEST = 2,
	AUTH_TYPE_0K     = 4
} AuthType;

static void     connection_free (LmConnection *connection);


static void     connection_handle_message    (LmConnection         *connection,
					      LmMessage            *message);

static void     connection_new_message_cb    (LmParser             *parser,
					      LmMessage            *message,
					      LmConnection         *connection);
static gboolean connection_do_open           (LmConnection         *connection,
					      GError              **error);

static void     connection_do_close           (LmConnection        *connection);
static gint     connection_do_write          (LmConnection         *connection,
					      const gchar          *buf,
					      gint                  len);

static gboolean connection_in_event          (GIOChannel   *source,
					      GIOCondition  condition,
					      LmConnection *connection);
static gboolean connection_error_event       (GIOChannel   *source,
					      GIOCondition  condition,
					      LmConnection *connection);
static gboolean connection_hup_event         (GIOChannel   *source,
					      GIOCondition  condition,
					      LmConnection *connection);
static gboolean connection_send              (LmConnection         *connection,
					      const gchar          *str,
					      gint                  len,
					      GError               **error);
static LmMessage *     connection_create_auth_req_msg (const gchar *username);
static LmMessage *     connection_create_auth_msg     (LmConnection *connection,
						       const gchar  *username,
						       const gchar  *password,
						       const gchar  *resource,
						       gint          auth_type);
static LmHandlerResult connection_auth_req_reply (LmMessageHandler *handler,
						  LmConnection     *connection,
						  LmMessage        *m,
						  gpointer          user_data);
static int connection_check_auth_type   (LmMessage           *auth_req_rpl);
					      
static LmHandlerResult connection_auth_reply (LmMessageHandler    *handler,
					      LmConnection        *connection,
					      LmMessage           *m,
					      gpointer             user_data);

static void     connection_stream_received   (LmConnection        *connection, 
					      LmMessage           *m);

static gint     connection_handler_compare_func (HandlerData  *a,
						 HandlerData  *b);
static gboolean connection_incoming_prepare  (GSource         *source,
					      gint            *timeout);
static gboolean connection_incoming_check    (GSource         *source);
static gboolean connection_incoming_dispatch (GSource         *source,
					      GSourceFunc      callback,
					      gpointer           user_data);
static GSource * connection_create_source       (LmConnection *connection);
static void      connection_signal_disconnect   (LmConnection *connection,
						 LmDisconnectReason reason);

static void     connection_do_connect           (LmConnectData *connect_data);
static guint    connection_add_watch            (LmConnection  *connection,
						 GIOChannel    *channel,
						 GIOCondition   condition,
						 GIOFunc        func,
						 gpointer       user_data);
static gboolean connection_send_keep_alive      (LmConnection  *connection);
static void     connection_start_keep_alive     (LmConnection  *connection);
static void     connection_stop_keep_alive      (LmConnection  *connection);
static gboolean connection_buffered_write_cb    (GIOChannel    *source, 
						 GIOCondition   condition,
						 LmConnection  *connection);
static gboolean connection_output_is_buffered   (LmConnection  *connection,
						 const gchar   *buffer,
						 gint           len);
static void     connection_setup_output_buffer  (LmConnection  *connection,
						 const gchar   *buffer,
						 gint           len);

static GSourceFuncs incoming_funcs = {
	connection_incoming_prepare,
	connection_incoming_check,
	connection_incoming_dispatch,
	NULL
};

static void
connection_free (LmConnection *connection)
{
	int        i;
	LmMessage *m;

	g_free (connection->server);
	g_free (connection->jid);
	g_free (connection->stream_id);

	if (connection->parser) {
		lm_parser_free (connection->parser);
	}

	/* Unref handlers */
	for (i = 0; i < LM_MESSAGE_TYPE_UNKNOWN; ++i) {
		GSList *l;

		for (l = connection->handlers[i]; l; l = l->next) {
			HandlerData *hd = (HandlerData *) l->data;
			
			lm_message_handler_unref (hd->handler);
			g_free (hd);
		}

		g_slist_free (connection->handlers[i]);
	}

	g_hash_table_destroy (connection->id_handlers);
	if (connection->state >= LM_CONNECTION_STATE_OPENING) {
		connection_do_close (connection);
	}

	if (connection->open_cb) {
		_lm_utils_free_callback (connection->open_cb);
	}
	
	if (connection->auth_cb) {
		_lm_utils_free_callback (connection->auth_cb);
	}

	lm_connection_set_disconnect_function (connection, NULL, NULL, NULL);

	while ((m = g_queue_pop_head (connection->incoming_messages)) != NULL) {
		lm_message_unref (m);
	}

	if (connection->ssl) {
		lm_ssl_unref (connection->ssl);
	}

	if (connection->proxy) {
		lm_proxy_unref (connection->proxy);
	}

	g_queue_free (connection->incoming_messages);
        
        if (connection->context) {
                g_main_context_unref (connection->context);
        }

	if (connection->out_buf) {
		g_string_free (connection->out_buf, TRUE);
	}
        
        g_free (connection);
}

static void
connection_handle_message (LmConnection *connection, LmMessage *m)
{
	LmMessageHandler *handler;
	GSList           *l;
	const gchar      *id;
	LmHandlerResult   result = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	lm_connection_ref (connection);

	if (lm_message_get_type (m) == LM_MESSAGE_TYPE_STREAM) {
		connection_stream_received (connection, m);
		goto out;
	}
	
	id = lm_message_node_get_attribute (m->node, "id");
	
	if (id) {
		handler = g_hash_table_lookup (connection->id_handlers, id);
		if (handler) {
			result = _lm_message_handler_handle_message (handler, 
								     connection,
								     m);
			g_hash_table_remove (connection->id_handlers, id);
		}
	}
	
	if (result == LM_HANDLER_RESULT_REMOVE_MESSAGE) {
		goto out;
	}

	for (l = connection->handlers[lm_message_get_type (m)]; 
	     l && result == LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; 
	     l = l->next) {
		HandlerData *hd = (HandlerData *) l->data;
		
		result = _lm_message_handler_handle_message (hd->handler,
							     connection,
							     m);
	}

out:
	lm_connection_unref (connection);
	
	return;
}

static void
connection_new_message_cb (LmParser     *parser,
			   LmMessage    *m,
			   LmConnection *connection)
{
	const gchar *from;
	
	lm_message_ref (m);

	from = lm_message_node_get_attribute (m->node, "from");
	if (!from) {
		from = "unknown";
	}
	
	lm_verbose ("New message with type=\"%s\" from: %s\n",
		    _lm_message_type_to_string (lm_message_get_type (m)),
		    from);

	g_queue_push_tail (connection->incoming_messages, m);
}

void
_lm_connection_succeeded (LmConnectData *connect_data)
{
	LmConnection *connection;
	LmMessage    *m;
	gchar        *server_from_jid;
	gchar        *ch;

	connection = connect_data->connection;
	
	if (connection->io_watch_connect != 0) {
		GSource *source;

		source = g_main_context_find_source_by_id (connection->context,
							   connection->io_watch_connect);
		if (source) {
			g_source_destroy (source);
		}
		connection->io_watch_connect = 0;
	}

	/* Need some way to report error/success */
	if (connection->cancel_open) {
		lm_verbose ("Cancelling connection...\n");
		return;
	}
	
	connection->fd = connect_data->fd;
	connection->io_channel = connect_data->io_channel;

	freeaddrinfo (connect_data->resolved_addrs);
	connection->connect_data = NULL;
	g_free (connect_data);

	if (connection->ssl) {
		GError *error = NULL;

		lm_verbose ("Setting up SSL...\n");
	
#ifdef HAVE_GNUTLS
		/* GNU TLS requires the socket to be blocking */
		_lm_sock_set_blocking (connection->fd, TRUE);
#endif

		if (!_lm_ssl_begin (connection->ssl, connection->fd,
				    connection->server,
				    &error)) {
			lm_verbose ("Could not begin SSL\n");
				    
			if (error) {
				g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
				       "%s\n", error->message);
				g_error_free (error);
			}

 			_lm_sock_shutdown (connection->fd);
 			_lm_sock_close (connection->fd);

			connection_do_close (connection);

			return;
		}
	
#ifdef HAVE_GNUTLS
		_lm_sock_set_blocking (connection->fd, FALSE); 
#endif
	}

	connection->io_watch_in = 
		connection_add_watch (connection,
				      connection->io_channel,
				      G_IO_IN,
				      (GIOFunc) connection_in_event,
				      connection);

	/* FIXME: if we add these, we don't get ANY
	 * response from the server, this is to do with the way that
	 * windows handles watches, see bug #331214.
	 */
#ifndef G_OS_WIN32
		connection->io_watch_err =
			connection_add_watch (connection,
					      connection->io_channel,
					      G_IO_ERR,
					      (GIOFunc) connection_error_event,
					      connection);
		
		connection->io_watch_hup =
			connection_add_watch (connection,
					      connection->io_channel,
					      G_IO_HUP,
					      (GIOFunc) connection_hup_event,
					      connection);
#endif

	/* FIXME: Set up according to XMPP 1.0 specification */
	/*        StartTLS and the like */
	if (!connection_send (connection, 
			      "<?xml version='1.0' encoding='UTF-8'?>", -1,
			      NULL)) {
		lm_verbose ("Failed to send xml version and encoding\n");
		connection_do_close (connection);

		return;
	}

	if (connection->jid != NULL && (ch = strchr (connection->jid, '@')) != NULL) {
		server_from_jid = ch + 1;
	} else {
		server_from_jid = connection->server;
	}

	m = lm_message_new (server_from_jid, LM_MESSAGE_TYPE_STREAM);
	lm_message_node_set_attributes (m->node,
					"xmlns:stream", 
					"http://etherx.jabber.org/streams",
					"xmlns", "jabber:client",
					NULL);
	
	lm_verbose ("Opening stream...");

	if (!lm_connection_send (connection, m, NULL)) {
		lm_verbose ("Failed to send stream information\n");
		connection_do_close (connection);
	}
		
	lm_message_unref (m);
}

void 
_lm_connection_failed_with_error (LmConnectData *connect_data, int error) 
{
	LmConnection *connection;
	
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Connection failed: %s (error %d)\n",
	       _lm_sock_get_error_str (error), error);
	
	connection = connect_data->connection;
	
	connect_data->current_addr = connect_data->current_addr->ai_next;
	
	if (connection->io_watch_connect != 0) {
		GSource *source;

		source = g_main_context_find_source_by_id (connection->context,
							   connection->io_watch_connect);
		if (source) {
			g_source_destroy (source);
		}

		connection->io_watch_connect = 0;
	}

	if (connect_data->io_channel != NULL) {
		g_io_channel_unref (connect_data->io_channel);
		/* FIXME: need to check for last unref and close the socket */
	}
	
	if (connect_data->current_addr == NULL) {
		connection_do_close (connection);
		if (connection->open_cb) {
			LmCallback *cb = connection->open_cb;

			connection->open_cb = NULL;
			
			(* ((LmResultFunction) cb->func)) (connection, FALSE,
							   cb->user_data);
			_lm_utils_free_callback (cb);
		}
		
		freeaddrinfo (connect_data->resolved_addrs);
		connection->connect_data = NULL;
		g_free (connect_data);
	} else {
		/* try to connect to the next host */
		connection_do_connect (connect_data);
	}
}

void 
_lm_connection_failed (LmConnectData *connect_data)
{
	_lm_connection_failed_with_error (connect_data, 
					  _lm_sock_get_last_error());
}

static gboolean 
connection_connect_cb (GIOChannel   *source, 
		       GIOCondition  condition,
		       LmConnectData *connect_data) 
{
	LmConnection    *connection;
	struct addrinfo *addr;
	int              err;
	socklen_t        len;
	LmSocket         fd; 

	connection = connect_data->connection;
	addr = connect_data->current_addr;
	fd = g_io_channel_unix_get_fd (source);
	
	if (condition == G_IO_ERR) {
		len = sizeof (err);
		_lm_sock_get_error (fd, &err, &len);
		if (!_lm_sock_is_blocking_error (err)) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
			       "Connection failed.\n");

			_lm_connection_failed_with_error (connect_data, err);

			connection->io_watch_connect = 0;
			return FALSE;
		}
	}

	if (connection->async_connect_waiting) {
		gint res;

		fd = g_io_channel_unix_get_fd (source);

		res = _lm_sock_connect (fd, addr->ai_addr, (int)addr->ai_addrlen);  
		if (res < 0) {
			err = _lm_sock_get_last_error ();
			if (_lm_sock_is_blocking_success (err)) {
				connection->async_connect_waiting = FALSE;

				g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
				       "Connection success.\n");
				
				_lm_connection_succeeded (connect_data);
			}
			
			if (connection->async_connect_waiting && 
			    !_lm_sock_is_blocking_error (err)) {
				g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
				       "Connection failed.\n");

				_lm_sock_close (connect_data->fd);
				_lm_connection_failed_with_error (connect_data, err);

				connection->io_watch_connect = 0;
				return FALSE;
			}
		} 
	} else {		
		/* for blocking sockets, G_IO_OUT means we are connected */
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Connection success.\n");
		
		_lm_connection_succeeded (connect_data);
	}

 	return TRUE; 
}

static const char *
connection_condition_to_str (GIOCondition condition)
{
	static char buf[256];

	buf[0] = '\0';

	if(condition & G_IO_ERR)
		strcat(buf, "G_IO_ERR ");
	if(condition & G_IO_HUP)
		strcat(buf, "G_IO_HUP ");
	if(condition & G_IO_NVAL)
		strcat(buf, "G_IO_NVAL ");
	if(condition & G_IO_IN)
		strcat(buf, "G_IO_IN ");
	if(condition & G_IO_OUT)
		strcat(buf, "G_IO_OUT ");

	return buf;
}

static void
connection_do_connect (LmConnectData *connect_data) 
{
	LmConnection    *connection;
	LmSocket         fd;
	int              res, err;
	int              port;
	char             name[NI_MAXHOST];
	char             portname[NI_MAXSERV];
	struct addrinfo *addr;
	
	connection = connect_data->connection;
	addr = connect_data->current_addr;
 
	if (connection->proxy) {
		port = htons (lm_proxy_get_port (connection->proxy));
	} else {
		port = htons (connection->port);
	}
	
	((struct sockaddr_in *) addr->ai_addr)->sin_port = port;

	res = getnameinfo (addr->ai_addr,
			   (socklen_t)addr->ai_addrlen,
			   name,     sizeof (name),
			   portname, sizeof (portname),
			   NI_NUMERICHOST | NI_NUMERICSERV);
	
	if (res < 0) {
		_lm_connection_failed (connect_data);
		return;
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

		_lm_connection_failed (connect_data);

		return;
	}

	/* Even though it says _unix_new(), it is supported by glib on
	 * win32 because glib does some cool stuff to find out if it
	 * can treat it as a FD or a windows SOCKET.
	 */
	connect_data->fd = fd;
	connect_data->io_channel = g_io_channel_unix_new (fd);
	
	g_io_channel_set_encoding (connect_data->io_channel, NULL, NULL);
	g_io_channel_set_buffered (connect_data->io_channel, FALSE);

	_lm_sock_set_blocking (connect_data->fd, 
			       connection->blocking);
	
	if (connection->proxy) {
		connection->io_watch_connect =
		connection_add_watch (connection,
				      connect_data->io_channel,
				      G_IO_OUT|G_IO_ERR,
				      (GIOFunc) _lm_proxy_connect_cb, 
				      connect_data);
	} else {
		connection->io_watch_connect =
		connection_add_watch (connection,
				      connect_data->io_channel,
				      G_IO_OUT|G_IO_ERR,
				      (GIOFunc) connection_connect_cb,
				      connect_data);
	}

	connection->async_connect_waiting = !connection->blocking;

  	res = _lm_sock_connect (connect_data->fd, 
				addr->ai_addr, (int)addr->ai_addrlen);  
	if (res < 0) {
		err = _lm_sock_get_last_error ();
		if (!_lm_sock_is_blocking_error (err)) {
			_lm_sock_close (connect_data->fd);
			_lm_connection_failed_with_error (connect_data, err);

			return;
		}
	}
}

static guint
connection_add_watch (LmConnection *connection,
		      GIOChannel   *channel,
		      GIOCondition  condition,
		      GIOFunc       func,
		      gpointer      user_data)
{
	GSource *source;
	guint    id;
                                                                                
	g_return_val_if_fail (channel != NULL, 0);
                                                                                
	source = g_io_create_watch (channel, condition);
                                                                                
	g_source_set_callback (source, (GSourceFunc)func, user_data, NULL);
                                                                                
	id = g_source_attach (source, connection->context);

	g_source_unref (source);
  
	return id;
}

static gboolean
connection_send_keep_alive (LmConnection *connection)
{ 
	if (!connection_send (connection, " ", -1, NULL)) {
		lm_verbose ("Error while sending keep alive package!\n");
	}

	return TRUE;
}

static void
connection_start_keep_alive (LmConnection *connection)
{
	if (connection->keep_alive_id != 0) {
		connection_stop_keep_alive (connection);
	}

	if (connection->keep_alive_rate > 0) {
		connection->keep_alive_id =
			g_timeout_add (connection->keep_alive_rate,
				       (GSourceFunc) connection_send_keep_alive,
				       connection);
	}
}

static void
connection_stop_keep_alive (LmConnection *connection)
{
	if (connection->keep_alive_id != 0) {
		g_source_remove (connection->keep_alive_id);
	}

	connection->keep_alive_id = 0;
}

static gboolean
connection_buffered_write_cb (GIOChannel   *source, 
			      GIOCondition  condition,
			      LmConnection *connection)
{
	gint     b_written;
	GString *out_buf;
	/* FIXME: Do the writing */

	out_buf = connection->out_buf;
	if (!out_buf) {
		/* Should not be possible */
		return FALSE;
	}

	b_written = connection_do_write (connection, out_buf->str, out_buf->len);

	if (b_written < 0) {
		connection_error_event (connection->io_channel, 
					G_IO_HUP,
					connection);
		return FALSE;
	}

	g_string_erase (out_buf, 0, (gsize) b_written);
	if (out_buf->len == 0) {
		lm_verbose ("Output buffer is empty, going back to normal output\n");

		if (connection->io_watch_out != 0) {
			GSource *source;

			source = g_main_context_find_source_by_id (connection->context, 
								   connection->io_watch_out);
			if (source) {
				g_source_destroy (source);
			}

			connection->io_watch_out = 0;
		}

		g_string_free (out_buf, TRUE);
		connection->out_buf = NULL;
		return FALSE;
	}

	return TRUE;
}

static gboolean
connection_output_is_buffered (LmConnection *connection,
			       const gchar  *buffer,
			       gint          len)
{
	if (connection->out_buf) {
		lm_verbose ("Appending %d bytes to output buffer\n", len);
		g_string_append_len (connection->out_buf, buffer, len);
		return TRUE;
	}

	return FALSE;
}

static void
connection_setup_output_buffer (LmConnection *connection,
				const gchar  *buffer,
				gint          len)
{
	lm_verbose ("OUTPUT BUFFER ENABLED\n");

	connection->out_buf = g_string_new_len (buffer, len);

	connection->io_watch_out =
		connection_add_watch (connection,
				      connection->io_channel,
				      G_IO_OUT,
				      (GIOFunc) connection_buffered_write_cb,
				      connection);
}

/* Returns directly */
/* Setups all data needed to start the connection attempts */
static gboolean
connection_do_open (LmConnection *connection, GError **error) 
{
	struct addrinfo  req;
	struct addrinfo *ans;
	LmConnectData   *data;

	if (lm_connection_is_open (connection)) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is already open, call lm_connection_close() first");
		return FALSE;
	}

	if (!connection->server) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_FAILED,
			     "You need to set the server hostname in the call to lm_connection_new()");
		return FALSE;
	}

	/* source thingie for messages and stuff */
	connection->incoming_source = connection_create_source (connection);
	g_source_attach (connection->incoming_source, connection->context);
	
	lm_verbose ("Connecting to: %s:%d\n", 
		    connection->server, connection->port);

	memset (&req, 0, sizeof(req));

	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;
	
	connection->cancel_open = FALSE;
	connection->state = LM_CONNECTION_STATE_OPENING;
	connection->async_connect_waiting = FALSE;
	
	if (connection->proxy) {
		int          err;
		const gchar *proxy_server;

		proxy_server = lm_proxy_get_server (connection->proxy);

		/* Connect through proxy */
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Going to connect to proxy %s\n", proxy_server);

		err = getaddrinfo (proxy_server, NULL, &req, &ans);
		if (err != 0) {
			const char *str;

			str = _lm_sock_addrinfo_get_error_str (err);
			g_set_error (error,
				     LM_ERROR,                 
				     LM_ERROR_CONNECTION_FAILED,   
				     str);
			return FALSE;
		}
	} else { 
		int err;

		/* Connect directly */
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Going to connect to %s\n", 
		       connection->server);

		err = getaddrinfo (connection->server, 
				   NULL, &req, &ans);
		if (err != 0) {
			const char *str;

			str = _lm_sock_addrinfo_get_error_str (err);
			g_set_error (error,
				     LM_ERROR,                 
				     LM_ERROR_CONNECTION_FAILED,   
				     str);
			return FALSE;
		}
	}

	if (connection->ssl) {
		_lm_ssl_initialize (connection->ssl);
	}

	/* Prepare and do the nonblocking connection */
	data = g_new (LmConnectData, 1);

	data->connection     = connection;
	data->resolved_addrs = ans;
	data->current_addr   = ans;
	data->io_channel     = NULL;
	data->fd             = -1;

	connection->connect_data = data;

	connection_do_connect (data);
	return TRUE;
}
					
static void
connection_do_close (LmConnection *connection)
{
	GSource       *source;
	LmConnectData *data;

	connection_stop_keep_alive (connection);

	if (connection->io_watch_connect != 0) {

		source = g_main_context_find_source_by_id (connection->context,
							   connection->io_watch_connect);

		if (source) {
			g_source_destroy (source);
		}

		connection->io_watch_connect = 0;
	}
	
	data = connection->connect_data;
	if (data) {
		freeaddrinfo (data->resolved_addrs);
		connection->connect_data = NULL;
		g_free (data);
	}

	if (connection->io_channel) {
		if (connection->io_watch_in != 0) {
			source = g_main_context_find_source_by_id (connection->context,
								   connection->io_watch_in);
			if (source) {
				g_source_destroy (source);
			}
			
			connection->io_watch_in = 0;
		}

		if (connection->io_watch_err != 0) {
			source = g_main_context_find_source_by_id (connection->context, 
								   connection->io_watch_err);
			if (source) {
				g_source_destroy (source);
			}

			connection->io_watch_err = 0;
		}

		if (connection->io_watch_hup != 0) {
			source = g_main_context_find_source_by_id (connection->context, 
								   connection->io_watch_hup);

			if (source) {
				g_source_destroy (source);
			}
			
			connection->io_watch_hup = 0;
		}

		if (connection->io_watch_out != 0) {
			source = g_main_context_find_source_by_id (connection->context, 
								   connection->io_watch_out);

			if (source) {
				g_source_destroy (source);
			}

			connection->io_watch_out = 0;
		}


		g_io_channel_unref (connection->io_channel);
		connection->io_channel = NULL;

		connection->fd = -1;
	}

	if (connection->incoming_source) {
		g_source_destroy (connection->incoming_source);
		g_source_unref (connection->incoming_source);
		connection->incoming_source = NULL;
	}

	if (!lm_connection_is_open (connection)) {
		/* lm_connection_is_open is FALSE for state OPENING as well */
		connection->state = LM_CONNECTION_STATE_CLOSED;
		connection->async_connect_waiting = FALSE;
		return;
	}
	
	connection->state = LM_CONNECTION_STATE_CLOSED;
	connection->async_connect_waiting = FALSE;
	if (connection->ssl) {
		_lm_ssl_close (connection->ssl);
	}
}

static gint
connection_do_write (LmConnection *connection,
		     const gchar  *buf,
		     gint          len)
{
	gint b_written;

	if (connection->ssl) {
		b_written = _lm_ssl_send (connection->ssl, buf, len);
	} else {
		GIOStatus io_status = G_IO_STATUS_AGAIN;
		gsize     bytes_written;

		while (io_status == G_IO_STATUS_AGAIN) {
			io_status = g_io_channel_write_chars (connection->io_channel, 
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
connection_read_incoming (LmConnection *connection,
			  gchar        *buf,
			  gsize         buf_size,
			  gsize        *bytes_read,
			  gboolean     *hangup)
{
	GIOStatus status;

	*hangup = FALSE;

	if (connection->ssl) {
		status = _lm_ssl_read (connection->ssl, 
				       buf, buf_size - 1, bytes_read);
	} else {
		status = g_io_channel_read_chars (connection->io_channel,
						  buf, buf_size - 1,
						  bytes_read,
						  NULL);
	}

	if (status != G_IO_STATUS_NORMAL || *bytes_read < 0) {
		gint reason;
		
		switch (status) {
		case G_IO_STATUS_EOF:
			reason = LM_DISCONNECT_REASON_HUP;
			break;
		case G_IO_STATUS_AGAIN:
			/* No data readable but we didn't hangup */
			return FALSE;
			break;
		case G_IO_STATUS_ERROR:
			reason = LM_DISCONNECT_REASON_ERROR;
			break;
		default:
			reason = LM_DISCONNECT_REASON_UNKNOWN;
		}

		connection_do_close (connection);
		connection_signal_disconnect (connection, reason);

		/* Notify connection_in_event that we hangup the connection */
		*hangup = TRUE;
		
		return FALSE;
	}

	buf[*bytes_read] = '\0';

	/* There is more data to be read */
	return TRUE;
}

static gboolean
connection_in_event (GIOChannel   *source,
		     GIOCondition  condition,
		     LmConnection *connection)
{
	gchar    buf[IN_BUFFER_SIZE];
	gsize    bytes_read;
	gboolean hangup;

	if (connection->io_channel == NULL) {
		return FALSE;
	}

	while (connection_read_incoming (connection, buf, IN_BUFFER_SIZE,
					 &bytes_read, &hangup)) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "\nRECV [%d]:\n", 
		       (int)bytes_read);
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
		       "-----------------------------------\n");
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "'%s'\n", buf);
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
		       "-----------------------------------\n");

		lm_verbose ("Read: %d chars\n", (int)bytes_read);

		lm_parser_parse (connection->parser, buf);
	}

	if (hangup) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
connection_error_event (GIOChannel   *source,
			GIOCondition  condition,
			LmConnection *connection)
{
	lm_verbose ("Error event: %d->'%s'\n", 
		    condition, connection_condition_to_str (condition));

	if (!connection->io_channel) {
		return FALSE;
	}

	connection_do_close (connection);
	connection_signal_disconnect (connection, LM_DISCONNECT_REASON_ERROR);
	
	return TRUE;
}

static gboolean
connection_hup_event (GIOChannel   *source,
		      GIOCondition  condition,
		      LmConnection *connection)
{
	lm_verbose ("HUP event: %d->'%s'\n", 
		    condition, connection_condition_to_str (condition));

	if (!connection->io_channel) {
		return FALSE;
	}

	connection_do_close (connection);
	connection_signal_disconnect (connection, LM_DISCONNECT_REASON_HUP);
	
	return TRUE;
}

static gboolean
connection_send (LmConnection  *connection, 
		 const gchar   *str, 
		 gint           len, 
		 GError       **error)
{
	gint b_written;
	
	if (connection->state < LM_CONNECTION_STATE_OPENING) {
		g_log (LM_LOG_DOMAIN,LM_LOG_LEVEL_NET,
		       "Connection is not open.\n");

		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is not open, call lm_connection_open() first");
		return FALSE;
	}

	if (len == -1) {
		len = strlen (str);
	}

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "\nSEND:\n");
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
	       "-----------------------------------\n");
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "%s\n", str);
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
	       "-----------------------------------\n");

	/* Check to see if there already is an output buffer, if so, add to the
	   buffer and return */

	if (connection_output_is_buffered (connection, str, len)) {
		return TRUE;
	}

	b_written = connection_do_write (connection, str, len);


	if (b_written < 0) {
		connection_error_event (connection->io_channel, 
					G_IO_HUP,
					connection);
		return FALSE;
	}

	if (b_written < len) {
		connection_setup_output_buffer (connection, 
						str + b_written, 
						len - b_written);
	}

	return TRUE;
}

typedef struct {
	gchar        *username;
	gchar        *password;
	gchar        *resource;
} AuthReqData;

static void 
auth_req_data_free (AuthReqData *data) {
	g_free (data->username);
	g_free (data->password);
	g_free (data->resource);
	g_free (data);
}

static LmMessage *
connection_create_auth_req_msg (const gchar *username)
{
	LmMessage     *m;
	LmMessageNode *q_node;
	
	m = lm_message_new_with_sub_type (NULL, LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);
	q_node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (q_node,
					"xmlns", "jabber:iq:auth",
					NULL);
	lm_message_node_add_child (q_node, "username", username);

	return m;
}

static LmMessage *
connection_create_auth_msg (LmConnection *connection,
			    const gchar  *username,
			    const gchar  *password,
			    const gchar  *resource,
			    gint          auth_type)
{
	LmMessage     *auth_msg;
	LmMessageNode *q_node;

	auth_msg = lm_message_new_with_sub_type (NULL, LM_MESSAGE_TYPE_IQ,
						 LM_MESSAGE_SUB_TYPE_SET);
	
	q_node = lm_message_node_add_child (auth_msg->node, "query", NULL);
	
	lm_message_node_set_attributes (q_node,
					"xmlns", "jabber:iq:auth", 
					NULL);

	lm_message_node_add_child (q_node, "username", username);
	
	if (auth_type & AUTH_TYPE_0K) {
		lm_verbose ("Using 0k auth (not implemented yet)\n");
		/* TODO: Should probably use this? */
	}

	if (auth_type & AUTH_TYPE_DIGEST) {
		gchar       *str;
		const gchar *digest;

		lm_verbose ("Using digest\n");
		str = g_strconcat (connection->stream_id, password, NULL);
		digest = lm_sha_hash (str);
		g_free (str);
		lm_message_node_add_child (q_node, "digest", digest);
	} 
	else if (auth_type & AUTH_TYPE_PLAIN) {
		lm_verbose ("Using plaintext auth\n");
		lm_message_node_add_child (q_node, "password", password);
	} else {
		/* TODO: Report error somehow */
	}
	
	lm_message_node_add_child (q_node, "resource", resource);

	return auth_msg;
}

static LmHandlerResult
connection_auth_req_reply (LmMessageHandler *handler,
			   LmConnection     *connection,
			   LmMessage        *m,
			   gpointer          user_data)
{
	int               auth_type;
	LmMessage        *auth_msg;
	LmMessageHandler *auth_handler;
	AuthReqData      *data = (AuthReqData *) user_data;      
	gboolean          result;
	
	auth_type = connection_check_auth_type (m);

	auth_msg = connection_create_auth_msg (connection, 
					       data->username,
					       data->password,
					       data->resource,
					       auth_type);

	auth_handler = lm_message_handler_new (connection_auth_reply,
					       NULL, NULL);
	result = lm_connection_send_with_reply (connection, auth_msg, 
						auth_handler, NULL);
	lm_message_handler_unref (auth_handler);
	lm_message_unref (auth_msg);
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static int
connection_check_auth_type (LmMessage *auth_req_rpl)
{
	LmMessageNode *q_node;
	gint           ret_val = 0; 

	q_node = lm_message_node_get_child (auth_req_rpl->node, "query");
	
	if (!q_node) {
		return AUTH_TYPE_PLAIN;
	}

	if (lm_message_node_get_child (q_node, "password")) {
		ret_val |= AUTH_TYPE_PLAIN;
	}

	if (lm_message_node_get_child (q_node, "digest")) {
		ret_val |= AUTH_TYPE_DIGEST;
	}

	if (lm_message_node_get_child (q_node, "sequence") &&
	    lm_message_node_get_child (q_node, "token")) {
		ret_val |= AUTH_TYPE_0K;
	}

	return ret_val;
}

static LmHandlerResult 
connection_auth_reply (LmMessageHandler *handler,
		       LmConnection     *connection,
		       LmMessage        *m,
		       gpointer          user_data)
{
	const gchar *type;
	gboolean     result = TRUE;
	
	g_return_val_if_fail (connection != NULL, 
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);
	

	type = lm_message_node_get_attribute (m->node, "type");
	if (strcmp (type, "result") == 0) {
		result = TRUE;
		connection->state = LM_CONNECTION_STATE_AUTHENTICATED;
	} 
	else if (strcmp (type, "error") == 0) {
		result = FALSE;
		connection->state = LM_CONNECTION_STATE_OPEN;
	}
	
	lm_verbose ("AUTH reply: %d\n", result);
	
	if (connection->auth_cb) {
	        LmCallback *cb = connection->auth_cb;

		connection->auth_cb = NULL;

		if (cb->func) {
	    		(* ((LmResultFunction) cb->func)) (connection, 
						           result, cb->user_data);
		}

		_lm_utils_free_callback (cb);
	}
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}


static void
connection_stream_received (LmConnection *connection, LmMessage *m)
{
	gboolean result;
	
	g_return_if_fail (connection != NULL);
	g_return_if_fail (m != NULL);
	
	connection->stream_id = g_strdup (lm_message_node_get_attribute (m->node,
									 "id"));;
	
	lm_verbose ("Stream received: %s\n", connection->stream_id);
	
	connection->state = LM_CONNECTION_STATE_OPEN;
	
	/* Check to see if the stream is correctly set up */
	result = TRUE;

	connection_start_keep_alive (connection);

	if (connection->open_cb) {
		LmCallback *cb = connection->open_cb;

		connection->open_cb = NULL;
		
		if (cb->func) {
		        (* ((LmResultFunction) cb->func)) (connection, result,
			        			   cb->user_data);

		}
		_lm_utils_free_callback (connection->open_cb);
	}
}

static gint
connection_handler_compare_func (HandlerData *a, HandlerData *b)
{
	return b->priority - a->priority;
}

static gboolean 
connection_incoming_prepare (GSource *source, gint *timeout)
{
	LmConnection *connection;
	
	connection = ((LmIncomingSource *)source)->connection;
	
	return !g_queue_is_empty (connection->incoming_messages);
}

static gboolean
connection_incoming_check (GSource *source)
{
	return FALSE;
}

static gboolean
connection_incoming_dispatch (GSource *source, 
			      GSourceFunc callback, 
			      gpointer user_data)
{
	LmConnection *connection;
	LmMessage    *m;
	
	connection = ((LmIncomingSource *) source)->connection;

	m = (LmMessage *) g_queue_pop_head (connection->incoming_messages);
	
	if (m) {
		connection_handle_message (connection, m);
		lm_message_unref (m);
	}

	return TRUE;
}

static GSource *
connection_create_source (LmConnection *connection)
{
	GSource *source;
	
	source = g_source_new (&incoming_funcs, sizeof (LmIncomingSource));
	((LmIncomingSource *) source)->connection = connection;
	
	return source;
}

static void
connection_signal_disconnect (LmConnection       *connection,
			      LmDisconnectReason  reason)
{
	if (connection->disconnect_cb && connection->disconnect_cb->func) {
		LmCallback *cb = connection->disconnect_cb;
		
		(* ((LmDisconnectFunction) cb->func)) (connection,
						       reason,
						       cb->user_data);
	}
}

/**
 * lm_connection_new:
 * @server: The hostname to the server for the connection.
 * 
 * Creates a new closed connection. To open the connection call 
 * lm_connection_open(). @server can be #NULL but must be set before calling lm_connection_open().
 * 
 * Return value: A newly created LmConnection, should be unreffed with lm_connection_unref().
 **/
LmConnection *
lm_connection_new (const gchar *server)
{
	LmConnection *connection;
	gint          i;
	
	lm_debug_init ();
	_lm_sock_library_init ();

	connection = g_new0 (LmConnection, 1);

	if (server) {
		connection->server = _lm_utils_hostname_to_punycode (server);
	} else {
		connection->server = NULL;
	}

	connection->context           = NULL;
	connection->port              = LM_CONNECTION_DEFAULT_PORT;
	connection->jid               = NULL;
	connection->ssl               = NULL;
	connection->proxy             = NULL;
	connection->disconnect_cb     = NULL;
	connection->incoming_messages = g_queue_new ();
	connection->cancel_open       = FALSE;
	connection->state             = LM_CONNECTION_STATE_CLOSED;
	connection->keep_alive_id     = 0;
	connection->keep_alive_rate   = 0;
	connection->out_buf           = NULL;
	connection->connect_data      = NULL;
	
	connection->id_handlers = g_hash_table_new_full (g_str_hash, 
							 g_str_equal,
							 g_free, 
							 (GDestroyNotify) lm_message_handler_unref);
	connection->ref_count         = 1;
	
	for (i = 0; i < LM_MESSAGE_TYPE_UNKNOWN; ++i) {
		connection->handlers[i] = NULL;
	}

	connection->parser = lm_parser_new 
		((LmParserMessageFunction) connection_new_message_cb, 
		 connection, NULL);

	return connection;
}

/**
 * lm_connection_new_with_context:
 * @server: The hostname to the server for the connection.
 * @context: The context this connection should be running in.
 * 
 * Creates a new closed connection running in a certain context. To open the 
 * connection call #lm_connection_open. @server can be #NULL but must be set 
 * before calling #lm_connection_open.
 * 
 * Return value: A newly created LmConnection, should be unreffed with lm_connection_unref().
 **/
LmConnection *
lm_connection_new_with_context (const gchar *server, GMainContext *context)
{
	LmConnection *connection;

	connection = lm_connection_new (server);
	connection->context = context;

	if (context) {
        	g_main_context_ref (connection->context);
	}

	return connection;
}

/**
 * lm_connection_open:
 * @connection: #LmConnection to open
 * @function: Callback function that will be called when the connection is open.
 * @user_data: User data that will be passed to @function.
 * @notify: Function for freeing that user_data, can be NULL.
 * @error: location to store error, or %NULL
 * 
 * An async call to open @connection. When the connection is open @function will be called.
 * 
 * Return value: #TRUE if everything went fine, otherwise #FALSE.
 **/
gboolean
lm_connection_open (LmConnection      *connection, 
		    LmResultFunction   function,
		    gpointer           user_data,
		    GDestroyNotify     notify,
		    GError           **error)
{
	g_return_val_if_fail (connection != NULL, FALSE);
	
	connection->open_cb = _lm_utils_new_callback (function, 
						      user_data, notify);
	connection->blocking = FALSE;

	return connection_do_open (connection, error);
}

/**
 * lm_connection_open_and_block:
 * @connection: an #LmConnection to open
 * @error: location to store error, or %NULL
 * 
 * Opens @connection and waits until the stream is setup. 
 * 
 * Return value: #TRUE if no errors where encountered during opening and stream setup successfully, #FALSE otherwise.
 **/
gboolean
lm_connection_open_and_block (LmConnection *connection, GError **error)
{
	gboolean          result;
	LmConnectionState state;
	
	g_return_val_if_fail (connection != NULL, FALSE);

	connection->open_cb = NULL;
	connection->blocking = TRUE;

	result = connection_do_open (connection, error);

	if (result == FALSE) {
		return FALSE;
	}
		
	while ((state = lm_connection_get_state (connection)) == LM_CONNECTION_STATE_OPENING) {
		if (g_main_context_pending (connection->context)) {
			g_main_context_iteration (connection->context, TRUE);
		} else {
			/* Sleep for 1 millisecond */
			g_usleep (1000);
		}
	}

	if (lm_connection_is_open (connection)) {
		connection_start_keep_alive (connection);
		return TRUE;
	}

	/* Need to set the error here: LM-15 */
	g_set_error (error,
		     LM_ERROR,
		     LM_ERROR_CONNECTION_FAILED,
		     "Opening the connection failed");

	return FALSE;
}

/**
 * lm_connection_cancel_open:
 * @connection: an #LmConnection to cancel opening on
 *
 * Cancels the open operation of a connection. The connection should be in the state #LM_CONNECTION_STATE_OPENING.
 **/
void
lm_connection_cancel_open (LmConnection *connection)
{
	g_return_if_fail (connection != NULL);

	connection->cancel_open = TRUE;
}

/**
 * lm_connection_close:
 * @connection: #LmConnection to close 
 * @error: location to store error, or %NULL
 * 
 * A synchronous call to close the connection. When returning the connection is considered to be closed and can be opened again with lm_connection_open().
 * 
 * Return value: Returns #TRUE if no errors where detected, otherwise #FALSE.
 **/
gboolean
lm_connection_close (LmConnection      *connection, 
		     GError           **error)
{
	gboolean no_errors = TRUE;
	
	g_return_val_if_fail (connection != NULL, FALSE);

	if (connection->state == LM_CONNECTION_STATE_CLOSED) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is not open, call lm_connection_open() first");
		return FALSE;
	}

	lm_verbose ("Disconnecting from: %s:%d\n", 
		    connection->server, connection->port);
	
	if (lm_connection_is_open (connection)) {
		if (!connection_send (connection, "</stream:stream>", -1, error)) {
			no_errors = FALSE;
		}
		
		g_io_channel_flush (connection->io_channel, NULL);
	}
	
	connection_do_close (connection);
	connection_signal_disconnect (connection, LM_DISCONNECT_REASON_OK);
	
	return no_errors;
}

/**
 * lm_connection_authenticate:
 * @connection: #LmConnection to authenticate.
 * @username: Username used to authenticate.
 * @password: Password corresponding to @username.
 * @resource: Resource used for this connection.
 * @function: Callback called when authentication is finished.
 * @user_data: Userdata passed to @function when called.
 * @notify: Destroy function to free the memory used by @user_data, can be NULL.
 * @error: location to store error, or %NULL
 * 
 * Tries to authenticate a user against the server. The #LmResult in the result callback @function will say whether it succeeded or not. 
 * 
 * Return value: #TRUE if no errors where detected while sending the authentication message, #FALSE otherwise.
 **/
gboolean
lm_connection_authenticate (LmConnection      *connection,
			    const gchar       *username,
			    const gchar       *password,
			    const gchar       *resource,
			    LmResultFunction   function,
			    gpointer           user_data,
			    GDestroyNotify     notify,
			    GError           **error)
{
	LmMessage        *m;
	LmMessageHandler *handler;
	gboolean          result;
	AuthReqData      *data;
	
	g_return_val_if_fail (connection != NULL, FALSE);
	g_return_val_if_fail (username != NULL, FALSE);
	g_return_val_if_fail (password != NULL, FALSE);
	g_return_val_if_fail (resource != NULL, FALSE);

	if (!lm_connection_is_open (connection)) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is not open, call lm_connection_open() first");
		return FALSE;
	}

	/* FIXME: Do SASL authentication here (if XMPP 1.0 is used) */

	connection->state = LM_CONNECTION_STATE_AUTHENTICATING;
	
	connection->auth_cb = _lm_utils_new_callback (function, 
						      user_data, 
						      notify);

	m = connection_create_auth_req_msg (username);
		
	data = g_new0 (AuthReqData, 1);
	data->username = g_strdup (username);
	data->password = g_strdup (password);
	data->resource = g_strdup (resource);
	
	handler = lm_message_handler_new (connection_auth_req_reply, 
					  data, 
					  (GDestroyNotify) auth_req_data_free);
	result = lm_connection_send_with_reply (connection, m, handler, error);
	
	lm_message_handler_unref (handler);
	lm_message_unref (m);

	return result;
}

/**
 * lm_connection_authenticate_and_block:
 * @connection: an #LmConnection
 * @username: Username used to authenticate.
 * @password: Password corresponding to @username.
 * @resource: Resource used for this connection.
 * @error: location to store error, or %NULL
 * 
 * Tries to authenticate a user against the server. This function blocks until a reply to the authentication attempt is returned and returns whether it was successful or not.
 * 
 * Return value: #TRUE if no errors where detected and authentication was successful. #FALSE otherwise.
 **/
gboolean
lm_connection_authenticate_and_block (LmConnection  *connection,
				      const gchar   *username,
				      const gchar   *password,
				      const gchar   *resource,
				      GError       **error)
{
	LmMessage        *m;
	LmMessage        *result;
	LmMessageSubType  type;
		
	g_return_val_if_fail (connection != NULL, FALSE);
	g_return_val_if_fail (username != NULL, FALSE);
	g_return_val_if_fail (password != NULL, FALSE);
	g_return_val_if_fail (resource != NULL, FALSE);

	if (!lm_connection_is_open (connection)) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is not open, call lm_connection_open() first");
		return FALSE;
	}

	connection->state = LM_CONNECTION_STATE_AUTHENTICATING;

	m = connection_create_auth_req_msg (username);
	result = lm_connection_send_with_reply_and_block (connection, m, error);
	lm_message_unref (m);

	if (!result) {
		connection->state = LM_CONNECTION_STATE_OPEN;
		return FALSE;
	}

	m = connection_create_auth_msg (connection,
					username, 
					password,
					resource,
					connection_check_auth_type (result));
	lm_message_unref (result);

	result = lm_connection_send_with_reply_and_block (connection, m, error);
	lm_message_unref (m);

	if (!result) {
		connection->state = LM_CONNECTION_STATE_OPEN;
		return FALSE;
	}

	type = lm_message_get_sub_type (result);
	lm_message_unref (result);
	
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_RESULT:
		connection->state = LM_CONNECTION_STATE_AUTHENTICATED;
		return TRUE;
		break;
	case LM_MESSAGE_SUB_TYPE_ERROR:
		connection->state = LM_CONNECTION_STATE_OPEN;
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_AUTH_FAILED,
			     "Authentication failed");
		return FALSE;
		break;
	default:
		g_assert_not_reached ();
		break;
	} 

	return FALSE;
}

/**
 * lm_connection_set_keep_alive_rate:
 * @connection: #LmConnection to check if it is open.
 * @rate: Number of seconds between keep alive packages are sent.
 * 
 * Set the keep alive rate, in seconds. Set to 0 to prevent keep alive messages to be sent.
 * A keep alive message is a single space character.
 **/
void
lm_connection_set_keep_alive_rate (LmConnection *connection, guint rate)
{
	g_return_if_fail (connection != NULL);

	connection_stop_keep_alive (connection);

	if (rate == 0) {
		connection->keep_alive_id = 0;
		return;
	}

	connection->keep_alive_rate = rate * 1000;
	
	if (lm_connection_is_open (connection)) {
		connection_start_keep_alive (connection);
	}
}

/**
 * lm_connection_is_open:
 * @connection: #LmConnection to check if it is open.
 * 
 * Check if the @connection is currently open.
 * 
 * Return value: #TRUE if connection is open and #FALSE if it is closed.
 **/
gboolean
lm_connection_is_open (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, FALSE);
	
	return connection->state >= LM_CONNECTION_STATE_OPEN;
}

/**
 * lm_connection_is_authenticated:
 * @connection: #LmConnection to check if it is authenticated
 * 
 * Check if @connection is authenticated.
 * 
 * Return value: #TRUE if connection is authenticated, #FALSE otherwise.
 **/
gboolean 
lm_connection_is_authenticated (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, FALSE);

	return connection->state >= LM_CONNECTION_STATE_AUTHENTICATED;
}

/**
 * lm_connection_get_server:
 * @connection: an #LmConnection
 * 
 * Fetches the server address that @connection is using. 
 * 
 * Return value: the server address
 **/
const gchar *
lm_connection_get_server (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, NULL);

	return connection->server;
}

/**
 * lm_connection_set_server:
 * @connection: an #LmConnection
 * @server: Address of the server
 * 
 * Sets the server address for @connection to @server. Notice that @connection
 * can't be open while doing this. 
 **/
void
lm_connection_set_server (LmConnection *connection, const gchar *server)
{
	g_return_if_fail (connection != NULL);
	g_return_if_fail (server != NULL);
	
	if (lm_connection_is_open (connection)) {
		g_warning ("Can't change server address while connected");
		return;
	}
	
	g_free (connection->server);
	connection->server = _lm_utils_hostname_to_punycode (server);
}

/**
 * lm_connection_get_jid:
 * @connection: an #LmConnection
 * 
 * Fetches the jid set for @connection is using. 
 * 
 * Return value: the jid
 **/
const gchar *
lm_connection_get_jid (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, NULL);

	return connection->jid;
}

/**
 * lm_connection_set_jid:
 * @connection: an #LmConnection
 * @jid: JID to be used for @connection
 * 
 * Sets the JID to be used for @connection.
 **/
void 
lm_connection_set_jid (LmConnection *connection, const gchar *jid)
{
	g_return_if_fail (connection != NULL);

	if (lm_connection_is_open (connection)) {
		g_warning ("Can't change JID while connected");
		return;
	}

	g_free (connection->jid);
	connection->jid = g_strdup (jid);
}

/**
 * lm_connection_get_port:
 * @connection: an #LmConnection
 * 
 * Fetches the port that @connection is using.
 * 
 * Return value: 
 **/
guint
lm_connection_get_port (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, 0);

	return connection->port;
}

/**
 * lm_connection_set_port:
 * @connection: an #LmConnection
 * @port: server port
 * 
 * Sets the server port that @connection will be using.
 **/
void
lm_connection_set_port (LmConnection *connection, guint port)
{
	g_return_if_fail (connection != NULL);
	
	if (lm_connection_is_open (connection)) {
		g_warning ("Can't change server port while connected");
		return;
	}
	
	connection->port = port;
}

/**
 * lm_connection_get_ssl: 
 * @connection: an #LmConnection
 *
 * Returns the SSL struct if the connection is using one.
 * 
 * Return value: The ssl struct or %NULL if no proxy is used.
 **/
LmSSL *
lm_connection_get_ssl (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, NULL);
	
	return connection->ssl;
}

/**
 * lm_connection_set_ssl:
 * @connection: An #LmConnection
 * @ssl: An #LmSSL
 *
 * Sets SSL struct or unset if @ssl is %NULL. If set @connection will use SSL to for the connection.
 */
void
lm_connection_set_ssl (LmConnection *connection, LmSSL *ssl)
{
	g_return_if_fail (connection != NULL);
	g_return_if_fail (lm_ssl_is_supported () == TRUE);

	if (connection->ssl) {
		lm_ssl_unref (connection->ssl);
	}

	if (ssl) {
		connection->ssl = lm_ssl_ref (ssl);
	} else {
		connection->ssl = NULL;
	}
}

/**
 * lm_connection_get_proxy: 
 * @connection: an #LmConnection
 *
 * Returns the proxy if the connection is using one.
 * 
 * Return value: The proxy or %NULL if no proxy is used.
 **/
LmProxy *
lm_connection_get_proxy (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, NULL);

	return connection->proxy;
} 

/**
 * lm_connection_set_proxy: 
 * @connection: an #LmConnection
 * @proxy: an #LmProxy
 *
 * Sets the proxy to use for this connection. To unset pass #NULL.
 * 
 **/
void
lm_connection_set_proxy (LmConnection *connection, LmProxy *proxy)
{
	g_return_if_fail (connection != NULL);

	if (lm_connection_is_open (connection)) {
		g_warning ("Can't change server proxy while connected");
		return;
	}

	if (connection->proxy) {
		lm_proxy_unref (connection->proxy);
		connection->proxy = NULL;
	}

	if (proxy && lm_proxy_get_type (proxy) != LM_PROXY_TYPE_NONE) {
		connection->proxy = lm_proxy_ref (proxy);
	}
}

/**
 * lm_connection_send: 
 * @connection: #LmConnection to send message over.
 * @message: #LmMessage to send.
 * @error: location to store error, or %NULL
 * 
 * Asynchronous call to send a message.
 * 
 * Return value: Returns #TRUE if no errors where detected while sending, #FALSE otherwise.
 **/
gboolean
lm_connection_send (LmConnection  *connection, 
		    LmMessage     *message, 
		    GError       **error)
{
	gchar    *xml_str;
	gchar    *ch;
	gboolean  result;
	
	g_return_val_if_fail (connection != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);
	
	xml_str = lm_message_node_to_string (message->node);
	if ((ch = strstr (xml_str, "</stream:stream>"))) {
		*ch = '\0';
	}
	
	result = connection_send (connection, xml_str, -1, error);
	g_free (xml_str);

	return result;
}

/**
 * lm_connection_send_with_reply:
 * @connection: #LmConnection used to send message.
 * @message: #LmMessage to send.
 * @handler: #LmMessageHandler that will be used when a reply to @message arrives
 * @error: location to store error, or %NULL
 * 
 * Send a #LmMessage which will result in a reply. 
 * 
 * Return value: Returns #TRUE if no errors where detected while sending, #FALSE otherwise.
 **/
gboolean 
lm_connection_send_with_reply (LmConnection      *connection,
			       LmMessage         *message,
			       LmMessageHandler  *handler,
			       GError           **error)
{
	gchar *id;
	
	g_return_val_if_fail (connection != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);
	g_return_val_if_fail (handler != NULL, FALSE);

	if (lm_message_node_get_attribute (message->node, "id")) {
		id = g_strdup (lm_message_node_get_attribute (message->node, 
							      "id"));
	} else {
		id = _lm_utils_generate_id ();
		lm_message_node_set_attributes (message->node, "id", id, NULL);
	}
	
	g_hash_table_insert (connection->id_handlers, 
			     id, lm_message_handler_ref (handler));
	
	return lm_connection_send (connection, message, error);
}

/**
 * lm_connection_send_with_reply_and_block:
 * @connection: an #LmConnection
 * @message: an #LmMessage
 * @error: Set if error was detected during sending.
 * 
 * Send @message and wait for return.
 * 
 * Return value: The reply
 **/
LmMessage *
lm_connection_send_with_reply_and_block (LmConnection  *connection,
					 LmMessage     *message,
					 GError       **error)
{
	gchar     *id;
	LmMessage *reply = NULL;

	g_return_val_if_fail (connection != NULL, NULL);
	g_return_val_if_fail (message != NULL, NULL);

	if (connection->state < LM_CONNECTION_STATE_OPENING) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is not open, call lm_connection_open() first");
		return FALSE;
	}


	if (lm_message_node_get_attribute (message->node, "id")) {
		id = g_strdup (lm_message_node_get_attribute (message->node, 
							      "id"));
	} else {
		id = _lm_utils_generate_id ();
		lm_message_node_set_attributes (message->node, "id", id, NULL);
	}

	g_source_remove (g_source_get_id (connection->incoming_source));
	g_source_unref (connection->incoming_source);
	connection->incoming_source = NULL;

	lm_connection_send (connection, message, error);

	while (!reply) {
		const gchar *m_id;
		guint        n;

		g_main_context_iteration (connection->context, TRUE);
	
		if (g_queue_is_empty (connection->incoming_messages)) {
			continue;
		}

		for (n = 0; n < g_queue_get_length (connection->incoming_messages); n++) {
			LmMessage *m;

			m = (LmMessage *) g_queue_peek_nth (connection->incoming_messages, n);

			m_id = lm_message_node_get_attribute (m->node, "id");
			
			if (m_id && strcmp (m_id, id) == 0) {
				reply = m;
				g_queue_pop_nth (connection->incoming_messages,
						 n);
				break;
			}
		}
	}

	g_free (id);
	connection->incoming_source = connection_create_source (connection);
	g_source_attach (connection->incoming_source, connection->context);

	return reply;
}

/**
 * lm_connection_register_message_handler:
 * @connection: Connection to register a handler for.
 * @handler: Message handler to register.
 * @type: Message type that @handler will handle.
 * @priority: The priority in which to call @handler.
 * 
 * Registers a #LmMessageHandler to handle incoming messages of a certain type.
 * To unregister the handler call lm_connection_unregister_message_handler().
 **/
void
lm_connection_register_message_handler  (LmConnection       *connection,
					 LmMessageHandler   *handler,
					 LmMessageType       type,
					 LmHandlerPriority   priority)
{
	HandlerData      *hd;
	
	g_return_if_fail (connection != NULL);
	g_return_if_fail (handler != NULL);
	g_return_if_fail (type != LM_MESSAGE_TYPE_UNKNOWN);

	hd = g_new0 (HandlerData, 1);
	hd->priority = priority;
	hd->handler  = lm_message_handler_ref (handler);

	connection->handlers[type] = g_slist_insert_sorted (connection->handlers[type],
							    hd, 
							    (GCompareFunc) connection_handler_compare_func);
}

/**
 * lm_connection_unregister_message_handler:
 * @connection: Connection to unregister a handler for.
 * @handler: The handler to unregister.
 * @type: What type of messages to unregister this handler for.
 * 
 * Unregisters a handler for @connection. @handler will no longer be called 
 * when incoming messages of @type arrive.
 **/
void
lm_connection_unregister_message_handler (LmConnection      *connection,
					  LmMessageHandler  *handler,
					  LmMessageType      type)
{
	GSList *l, *prev = NULL;
	
	g_return_if_fail (connection != NULL);
	g_return_if_fail (handler != NULL);
	g_return_if_fail (type != LM_MESSAGE_TYPE_UNKNOWN);

	for (l = connection->handlers[type]; l; l = l->next) {
		HandlerData *hd = (HandlerData *) l->data;
		
		if (hd->handler == handler) {
			if (prev) {
				prev->next = l->next;
			} else {
				connection->handlers[type] = l->next;
			}
			l->next = NULL;
			g_slist_free (l);
			lm_message_handler_unref (hd->handler);
			g_free (hd);
			break;
		}
		prev = l;
	}
}

/**
 * lm_connection_set_disconnect_function:
 * @connection: Connection to register disconnect callback for.
 * @function: Function to be called when @connection is closed.
 * @user_data: User data passed to @function.
 * @notify: Function that will be called with @user_data when @user_data needs to be freed. Pass #NULL if it shouldn't be freed.
 * 
 * Set the callback that will be called when a connection is closed. 
 **/
void
lm_connection_set_disconnect_function (LmConnection         *connection,
				       LmDisconnectFunction  function,
				       gpointer              user_data,
				       GDestroyNotify        notify)
{
	g_return_if_fail (connection != NULL);

	if (connection->disconnect_cb) {
		_lm_utils_free_callback (connection->disconnect_cb);
	}
		
	if (function) {
		connection->disconnect_cb = _lm_utils_new_callback (function, 
	    					                    user_data,
							            notify);
	} else {
		connection->disconnect_cb = NULL;
	}
}

/**
 * lm_connection_send_raw:
 * @connection: Connection used to send
 * @str: The string to send, the entire string will be sent.
 * @error: Set if error was detected during sending.
 * 
 * Asynchronous call to send a raw string. Useful for debugging and testing.
 * 
 * Return value: Returns #TRUE if no errors was detected during sending, 
 * #FALSE otherwise.
 **/
gboolean 
lm_connection_send_raw (LmConnection  *connection, 
			const gchar   *str, 
			GError       **error)
{
	g_return_val_if_fail (connection != NULL, FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	return connection_send (connection, str, -1, error);
}
/**
 * lm_connection_get_state:
 * @connection: Connection to get state on
 *
 * Returns the state of the connection.
 *
 * Return value: The state of the connection.
 **/
LmConnectionState 
lm_connection_get_state (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, 
			      LM_CONNECTION_STATE_CLOSED);

	return connection->state;
}

/**
 * lm_connection_ref:
 * @connection: Connection to add a reference to.
 * 
 * Add a reference on @connection. To remove a reference call 
 * lm_connection_unref().
 * 
 * Return value: Returns the same connection.
 **/
LmConnection*
lm_connection_ref (LmConnection *connection)
{
	g_return_val_if_fail (connection != NULL, NULL);
	
	connection->ref_count++;
	
	return connection;
}

/**
 * lm_connection_unref:
 * @connection: Connection to remove reference from.
 * 
 * Removes a reference on @connection. If there are no references to
 * @connection it will be freed and shouldn't be used again.
 **/
void
lm_connection_unref (LmConnection *connection)
{
	g_return_if_fail (connection != NULL);
	
	connection->ref_count--;
	
	if (connection->ref_count == 0) {
		connection_free (connection);
	}
}

