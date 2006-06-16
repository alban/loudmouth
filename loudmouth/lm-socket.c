/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#include "lm-socket.h"

#ifndef G_OS_WIN32
typedef int LmSock;
#else  /* G_OS_WIN32 */
typedef SOCKET LmSock;
#endif /* G_OS_WIN32 */

/* FIXME: Integrate with the SSL stuff */

/* FIXME: IO Buffering both in/out here? */

struct _LmSocket {
	LmSock          sock;
	GIOChannel     *io_channel;
	guint           io_watch;
	/* FIXME: Add the rest */

	LmSSL          *ssl;
	
	LmSocketFuncs   funcs;
	
	LmSocketState   state;

	gboolean        is_blocking;
	
	gint            ref;
};

typedef void   (* SocketDNSCallback)  (LmSocket      *socket,
				       SocketDNSData *data,
				       gboolean       success);

typedef struct {
	/* Used when socket tries to connect */
	struct addrinfo   *resolved_addrs;
	struct addrinfo   *current_addr;

	/* -- Internal during DNS lookup -- */
	LmSocket          *socket;
	gchar             *host;
	SocketDNSCallback  callback;
} SocketDNSData;

static LmSocket *  socket_create            (void);
static void        socket_free              (LmSocket          *socket);
static gboolean    socket_channel_event     (GIOChannel        *source,
					     GIOCondition       condition,
					     LmSocket          *socket);
static void        socket_dns_lookup        (LmSocket          *socket,
					     const gchar       *host,
					     SocketDNSCallback  callback);
static SocketDNSData * socket_dns_data_new  (LmSocket          *socket,
					     const gchar       *host,
					     SocketDNSCallback  callbac);
static void            socket_dns_data_free (SocketDNSData     *data);
static void            socket_start_connect (LmSocket          *socket,
					     SocketDNSData     *data,
					     gboolean           success);

static LmSocket *
socket_create (void)
{
	LmSocket *socket;

	socket = g_new0 (LmSocket, 1);
	socket->ref_count = 1;
	socket->is_blocking = FALSE;
	socket->state = LM_SOCKET_STATE_CLOSED;
	socket->ssl = NULL;
	
	return socket;
}

static void
socket_free (LmSocket *socket)
{
	/* FIXME: Free up the rest of the memory */

	if (socket->io_channel) {
		if (socket->io_watch != 0) {
			g_source_destroy (g_main_context_find_source_by_id (socket->context),
					  socket->io_watch);
			socket->io_watch = 0;
		}

		g_io_channel_unref (socket->io_channel);
		socket->io_channel = NULL;

		socket->fd = -1;
	}

	if (socket->ssl) {
		_lm_ssl_unref (socket->ssl);
	}

	g_free (socket->host);
	g_free (socket);
}

static gboolean
socket_channel_event (GIOChannel   *source,
		      GIOCondition  condition,
		      LmSocket     *socket)
{
	if (condition & G_IO_IN)  {}
	if (condition & G_IO_OUT) {}
	if (condition & G_IO_ERR) {}
	if (condition & G_IO_HUP) {}
}

static void
eocket_dns_lookup (LmSocket          *socket,
		   const gchar       *host,
		   SocketDNSCallback  callback);
{
	SocketDNSData   *data;
	struct addrinfo  req;
	struct addrinfo  ans;
	int              err;

	/* FIXME: This should not be synchronous */
	data = socket_dns_data_new (socket, host, callback);

	memset (&req, 0, sizeof (req));
	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;

	err = getaddrinfo (data->host, NULL, &req, &ans);
	if (err != 0) {
		/* FIXME: Handle error */
		data->callback (data->socket, data, FALSE);
	}

	data->resolved_addrs = ans;
	data->current_addr   = ans;

	data->callback (data->socket, data, TRUE);

}

static SocketDNSData *
socket_dns_data_new (LmSocket          *socket,
		     const gchar       *host,
		     SocketDNSCallback  callbac)
{
	SocketDNSData *data;

	data = g_new0 (SocketDNSData, 1);
	
	data->socket   = lm_socket_ref (socket);
	data->host     = g_strdup (host);
	data->callback = callback;

	data->resolved_addrs = data->current_addr = NULL;

	return data;
}

static void
socket_dns_data_free (SocketDNSData *data)
{
	lm_socket_unref (data->socket);
	g_free (data->host);

	if (data->resolved_addrs) {
		freeaddrinfo (data->resolved_addrs);;
		deta->resolved_addrs = NULL;
	}

	g_free (data);
}

static void
socket_start_connect (LmSocket      *socket,
		      SocketDNSData *data,
		      gboolean       success)
{
	if (!success) {
		socket_dns_data_free (data);
		/* FIXME: Report error */
	}

	/* FIXME: Continue and connect */
}

LmSocket *
lm_socket_new (LmSocketFuncs funcs, const gchar *host, guint port)
{
	LmSocket *socket;

	socket = socket_create ();

	socket->funcs = funcs;
	socket->host = g_strdup (host);
	socket->port = port;
	socket->is_blocking = FALSE;

	return socket;
}

void
lm_socket_open (LmSocket *socket)
{
	g_return_if_fail (socket != NULL);

	socket_dns_lookup (socket, socket->host, socket_start_connect);
}

int
lm_socket_get_fd (LmSocket *socket)
{
	g_return_val_if_fail (socket != NULL, -1);

	return socket->fd;
}

gboolean
lm_socket_get_is_blocking (LmSocket *socket)
{
	return socket->is_blocking;
}

void
lm_socket_set_is_blocking (LmSocket *socket, gboolean is_block)
{
	int res;

	g_return_if_fail (socket != NULL);

	/* FIXME: Don't unset all flags */

#ifndef G_OS_WIN32
	res = fcntl (socket->sock, F_SETFL, is_block ? 0 : O_NONBLOCK);
#else  /* G_OS_WIN32 */
	u_long mode = (is_block ? 0 : 1);
	res = ioctlsocket (socket->sock, FIONBIO, &mode);
#endif /* G_OS_WIN32 */

	if (res != 0) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Could not set socket to be %s\n",
		       block ? "blocking" : "non-blocking");
	}

	socket->is_blocking = is_block;
}

int
lm_socket_write (LmSocket   *socket,
		 gsize       size,
		 gchar      *buf,
		 GError    **error)
{
	gint b_written;
	
	g_return_val_if_fail (socket != NULL, -1);

	if (socket->ssl) {
		b_written = _lm_ssl_send (socket->ssl, buf, len);
	} else {
		GIOStatus io_status = G_IO_STATUS_AGAIN;
		gsize     bytes_written = 0;

		while (io_status == G_IO_STATUS_AGAIN) {
			io_status = g_io_channel_write_chars (socket->io_channel,
							      buf, size,
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

int
lm_socket_read (LmSocket   *socket,
		gsize       size,
		gchar      *buf,
		GError    **error)
{
	gsize     bytes_read = 0;
	GIOStatus status =  G_IO_STATUS_AGAIN;

	g_return_val_if_fail (socket != NULL, -1);

	while (status == G_IO_STATUS_AGAIN) {
		if (socket->ssl) {
			status = _lm_ssl_read (socket->ssl, 
					       buf, size, &bytes_read);
		} else {
			status = g_io_channel_read_chars (socket->io_channel, 
							  buf, size, 
							  &bytes_read,
							  NULL);
		}
	}

	if (status != G_IO_STATUS_NORMAL || bytes_read < 0) {
		/* FIXME: Set error */

		return -1;
	}

	return bytes_read;
}

gboolean
lm_socket_close (LmSocket *socket, GError **error)
{
#ifndef G_OS_WIN32
	close (socket->sock);
#else /* G_OS_WIN32 */
	closesocket (socket->sock);
#endif /* G_OS_WIN32 */
}

LmSocket *
lm_socket_ref (LmSocket *socket)
{
	g_return_val_if_fail (socket != NULL, NULL);

	socket->ref++;
	
	return socket;
}

void
lm_socket_unref (LmSocket *socket)
{
	g_return_if_fail (socket != NULL);
	
	socket->ref--;
	
	if (socket->ref <= 0) {
		socket_free (socket);
	}
}

