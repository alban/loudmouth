/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
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
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#ifndef __WIN32__
  #include <netdb.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <sys/time.h>
#else
  #include <winsock2.h>
#endif

#include "lm-debug.h"
#include "lm-error.h"
#include "lm-internals.h"
#include "lm-parser.h"
#include "lm-sha.h"
#include "lm-connection.h"

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
	guint         fd;
	
	guint         open_id;
	LmCallback   *open_cb;

	gboolean      cancel_open;
	LmCallback   *close_cb;
	LmCallback   *auth_cb;
	LmCallback   *register_cb;

	LmCallback   *disconnect_cb;

	GQueue       *incoming_messages;
	GSource      *incoming_source;

	LmConnectionState state;

	guint         keep_alive_rate;
	guint         keep_alive_id;

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
static gboolean connection_in_event          (GIOChannel   *source,
					      GIOCondition  condition,
					      LmConnection *connection);
static gboolean connection_error_event       (GIOChannel   *source,
					      GIOCondition  condition,
					      LmConnection *connection);
static gboolean connection_hup_event         (GIOChannel   *source,
					      GIOCondition  condition,
					      LmConnection *connection);
static gboolean connection_send              (LmConnection             *connection,
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

static void     connection_stream_received   (LmConnection             *connection, 
					      LmMessage                *m);

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

static GSourceFuncs incoming_funcs = {
	connection_incoming_prepare,
	connection_incoming_check,
	connection_incoming_dispatch,
	NULL
};

static void
connection_free (LmConnection *connection)
{
	int i;

	g_free (connection->server);
	g_free (connection->jid);

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

	g_free (connection);
}

static void
connection_handle_message (LmConnection *connection, LmMessage *m)
{
	LmMessageHandler *handler;
	GSList           *l;
	const gchar      *id;
	LmHandlerResult   result = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	if (lm_message_get_type (m) == LM_MESSAGE_TYPE_STREAM) {
		connection_stream_received (connection, m);
		return;
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
		return;
	}

	for (l = connection->handlers[lm_message_get_type (m)]; 
	     l && result == LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; 
	     l = l->next) {
		HandlerData *hd = (HandlerData *) l->data;
		
		result = _lm_message_handler_handle_message (hd->handler,
							     connection,
							     m);
	}
	
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

gboolean
_lm_connection_succeeded (LmConnectData *connect_data)
{
	LmConnection *connection;
	LmMessage    *m;
	GIOFlags      flags;
	gchar        *server_from_jid;
	gchar        *ch;

	connection = connect_data->connection;

	/* Need some way to report error/success */
	if (connection->cancel_open) {
		return FALSE;
	}
	
	connection->fd = connect_data->fd;
	connection->io_channel = connect_data->io_channel;

	freeaddrinfo (connect_data->resolved_addrs);

	/* don't need this anymore */
	g_free(connect_data);

	flags = g_io_channel_get_flags (connection->io_channel);

	/* unset the nonblock flag */
	flags &= ~G_IO_FLAG_NONBLOCK;
	
	/* unset the nonblocking stuff for some time, because GNUTLS doesn't 
	 * like that */
	g_io_channel_set_flags (connection->io_channel, flags, NULL);

	if (connection->ssl) {
		if (!_lm_ssl_begin (connection->ssl, connection->fd,
				    connection->server,
				    NULL)) {
			shutdown (connection->fd, SHUT_RDWR);
			close (connection->fd);
			connection_do_close (connection);
			connection->fd = -1;
			g_io_channel_unref(connection->io_channel);
			return FALSE;
		}
	}
	
	g_io_channel_set_close_on_unref (connection->io_channel, TRUE);
	g_io_channel_set_encoding (connection->io_channel, NULL, NULL);
	
	g_io_channel_set_buffered (connection->io_channel, FALSE);
	g_io_channel_set_flags (connection->io_channel,
				flags & G_IO_FLAG_NONBLOCK, NULL);
	
	connection->io_watch_in = 
		connection_add_watch (connection,
				      connection->io_channel,
				      G_IO_IN,
				      (GIOFunc) connection_in_event,
				      connection);
	
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
	
	if (!connection_send (connection, 
			      "<?xml version='1.0' encoding='UTF-8'?>", -1,
			      NULL)) {
		connection_do_close (connection);
		return FALSE;
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
		lm_message_unref (m);
		connection_do_close (connection);
		return FALSE;
	}
		
	lm_message_unref (m);

	/* Success */
	return FALSE;
}

void 
_lm_connection_failed_with_error (LmConnectData *connect_data, int error) 
{
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Connection failed: %s (error %d)\n",
	       strerror (error), error);
	
	connect_data->current_addr = connect_data->current_addr->ai_next;
	
	if (connect_data->io_channel != NULL) {
		g_io_channel_unref (connect_data->io_channel);
	}
	
	if (connect_data->current_addr == NULL) {
		LmConnection *connection;

		connection = connect_data->connection;
		if (connection->open_cb && connection->open_cb->func) {
			LmCallback *cb = connection->open_cb;
			
			(* ((LmResultFunction) cb->func)) (connection, FALSE,
							   cb->user_data);
		}

		freeaddrinfo (connect_data->resolved_addrs);
		g_free (connect_data);
	} else {
		/* try to connect to the next host */
		connection_do_connect (connect_data);
	}
}

void 
_lm_connection_failed (LmConnectData *connect_data)
{
	_lm_connection_failed_with_error (connect_data,errno);
}
	
static gboolean 
connection_connect_cb (GIOChannel   *source, 
		       GIOCondition  condition,
		       gpointer      data) 
{
	LmConnection  *connection;
	LmConnectData *connect_data;
	int            error;
	int            len  = sizeof(error);

	connect_data = (LmConnectData *) data;
	connection   = connect_data->connection;
	
	if (condition == G_IO_ERR) {
		/* get the real error from the socket */
		getsockopt (connect_data->fd, SOL_SOCKET, SO_ERROR, 
			    &error, &len);
		_lm_connection_failed_with_error (connect_data, error);
		return FALSE;
	} else if (condition == G_IO_OUT) {
		_lm_connection_succeeded (connect_data);
	} else {
		g_assert_not_reached ();
	}

	return FALSE;
}

static void
connection_do_connect (LmConnectData *connect_data) 
{
	LmConnection    *connection;
	int              fd;
	int              res;
	int              port;
	int              flags;
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

	getnameinfo (addr->ai_addr,
		     addr->ai_addrlen,
		     name,     sizeof (name),
		     portname, sizeof (portname),
		     NI_NUMERICHOST | NI_NUMERICSERV);

	g_log (LM_LOG_DOMAIN,LM_LOG_LEVEL_NET,
	       "Trying %s port %s...\n", name, portname);
	
	fd = socket (addr->ai_family, 
		     addr->ai_socktype, 
		     addr->ai_protocol);
	
	if (fd < 0) {
		_lm_connection_failed (connect_data);
		return;
	}

	flags = fcntl (fd, F_GETFL, 0);
	fcntl (fd, F_SETFL, flags | O_NONBLOCK);
	
	res = connect (fd, addr->ai_addr, addr->ai_addrlen);
	connect_data->fd = fd;
	
	if (res < 0 && errno != EINPROGRESS) {
		close (fd);
		_lm_connection_failed (connect_data);
		return;
	}
	
	connect_data->io_channel = g_io_channel_unix_new (fd);
	if (connection->proxy) {
		connection_add_watch (connection,
				      connect_data->io_channel,
				      G_IO_OUT|G_IO_ERR,
				      (GIOFunc) _lm_proxy_connect_cb, 
				      connect_data);
	} else {
		connection_add_watch (connection,
				      connect_data->io_channel,
				      G_IO_OUT|G_IO_ERR,
				      (GIOFunc) connection_connect_cb,
				      connect_data);
	}

	return;
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
		lm_verbose ("Error while sending keep alive package");
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
			     LM_ERROR_CONNECTION_OPEN,
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
	
	if (connection->proxy) {
		const gchar *proxy_server;

		proxy_server = lm_proxy_get_server (connection->proxy);
		/* connect through proxy */
		g_log (LM_LOG_DOMAIN,LM_LOG_LEVEL_NET,
		       "Going to connect to %s\n", proxy_server);

		if (getaddrinfo (proxy_server, NULL, &req, &ans) != 0) {
			g_set_error (error,
				     LM_ERROR,                 
				     LM_ERROR_CONNECTION_OPEN,   
				     "getaddrinfo() failed");
			return FALSE;
		}
	} else { /* connect directly */
		g_log (LM_LOG_DOMAIN,LM_LOG_LEVEL_NET,
		       "Going to connect to %s\n", 
		       connection->server);

		if (getaddrinfo (connection->server,
				 NULL, &req, &ans) != 0) {
			g_set_error (error,
				     LM_ERROR,                 
				     LM_ERROR_CONNECTION_OPEN,   
				     "getaddrinfo() failed");
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

	connection_do_connect (data);
	return TRUE;
}
					
static void
connection_do_close (LmConnection *connection)
{
	connection_stop_keep_alive (connection);

	if (connection->io_channel) {
		g_source_destroy (g_main_context_find_source_by_id (
			connection->context, connection->io_watch_in));
		g_source_destroy (g_main_context_find_source_by_id (
			connection->context, connection->io_watch_err));
		g_source_destroy (g_main_context_find_source_by_id (
			connection->context, connection->io_watch_hup));

		g_io_channel_unref (connection->io_channel);
		connection->io_channel = NULL;
	}

	g_source_destroy (connection->incoming_source);
	g_source_unref (connection->incoming_source);

	if (!lm_connection_is_open (connection)) {
		return;
	}
	
	connection->state = LM_CONNECTION_STATE_CLOSED;

	if (connection->ssl) {
		_lm_ssl_close (connection->ssl);
	}
}


static gboolean
connection_in_event (GIOChannel   *source,
		     GIOCondition  condition,
		     LmConnection *connection)
{
	gchar     buf[IN_BUFFER_SIZE];
	gsize     bytes_read;
	GIOStatus status;
       
	if (!connection->io_channel) {
		return FALSE;
	}

	if (connection->ssl) {
		status = _lm_ssl_read (connection->ssl, 
				       buf, IN_BUFFER_SIZE - 1, &bytes_read);
	} else {
		status = g_io_channel_read_chars (connection->io_channel,
						  buf, IN_BUFFER_SIZE - 1,
						  &bytes_read,
						  NULL);
	}

	if (status != G_IO_STATUS_NORMAL || bytes_read < 0) {
		gint reason;
		
		switch (status) {
		case G_IO_STATUS_EOF:
			reason = LM_DISCONNECT_REASON_HUP;
			break;
		case G_IO_STATUS_AGAIN:
			return TRUE;
			break;
		case G_IO_STATUS_ERROR:
			reason = LM_DISCONNECT_REASON_ERROR;
			break;
		default:
			reason = LM_DISCONNECT_REASON_UNKNOWN;
		}

		connection_do_close (connection);
		connection_signal_disconnect (connection, reason);
		
		return FALSE;
	}

	buf[bytes_read] = '\0';
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "\nRECV [%d]:\n", 
	       bytes_read);
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
	       "-----------------------------------\n");
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "'%s'\n", buf);
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, 
	       "-----------------------------------\n");

	lm_verbose ("Read: %d chars\n", bytes_read);

	lm_parser_parse (connection->parser, buf);
	
	return TRUE;
}

static gboolean
connection_error_event (GIOChannel   *source,
			GIOCondition  condition,
			LmConnection *connection)
{
	if (!connection->io_channel) {
		return FALSE;
	}

	lm_verbose ("Error event: %d\n", condition);
	
	connection_do_close (connection);
	connection_signal_disconnect (connection, LM_DISCONNECT_REASON_ERROR);
	
	return TRUE;
}

static gboolean
connection_hup_event (GIOChannel   *source,
		      GIOCondition  condition,
		      LmConnection *connection)
{
	if (!connection->io_channel) {
		return FALSE;
	}

	lm_verbose ("HUP event\n");

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
	gsize             bytes_written;
	
	if (connection->state < LM_CONNECTION_STATE_OPENING) {
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

	if (connection->ssl) {
		if (!_lm_ssl_send (connection->ssl, str, len)) {
			
			connection_error_event (connection->io_channel, 
						G_IO_HUP,
						connection);
		}
	} else {
		g_io_channel_write_chars (connection->io_channel, str, len, 
					  &bytes_written, NULL);
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
	
	if (connection->auth_cb && connection->auth_cb->func) {
		LmCallback *cb = connection->auth_cb;

		(* ((LmResultFunction) cb->func)) (connection, 
						   result, cb->user_data);
	}
	
	_lm_utils_free_callback (connection->auth_cb);
	connection->auth_cb = NULL;
	
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

	if (connection->open_cb && connection->open_cb->func) {
		LmCallback *cb = connection->open_cb;
		
		(* ((LmResultFunction) cb->func)) (connection, result,
						   cb->user_data);
	}
	
	_lm_utils_free_callback (connection->open_cb);
	connection->open_cb = NULL;
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
	
	connection = g_new0 (LmConnection, 1);

	if (server) {
		connection->server = g_strdup (server);
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

	g_main_context_ref (connection->context);

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
	result = connection_do_open (connection, error);

	if (result == FALSE) {
		return FALSE;
	}
	
	while ((state = lm_connection_get_state (connection)) == LM_CONNECTION_STATE_OPENING) {
		if (g_main_context_pending (connection->context)) {
			g_main_context_iteration (connection->context, TRUE);
		} else {
			usleep (10);
		}
	}

	if (lm_connection_is_open (connection)) {
		connection_start_keep_alive (connection);
		return TRUE;
	}

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
	g_return_val_if_fail (connection != NULL, FALSE);
	
	if (!lm_connection_is_open (connection)) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_NOT_OPEN,
			     "Connection is not open, call lm_connection_open() first");
		return FALSE;
	}

	lm_verbose ("Disconnecting from: %s:%d\n", 
		    connection->server, connection->port);
	
	if (!connection_send (connection, "</stream:stream>", -1, error)) {
		return FALSE;
	}
	
 	g_io_channel_flush (connection->io_channel, NULL);
	
	connection_do_close (connection);
	connection_signal_disconnect (connection, LM_DISCONNECT_REASON_OK);
	
	return TRUE;
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

	m = connection_create_auth_req_msg (username);
	result = lm_connection_send_with_reply_and_block (connection, m, error);
	lm_message_unref (m);

	if (!result) {
		return FALSE;
	}

	m = connection_create_auth_msg (connection,
					username, 
					password,
					resource,
					connection_check_auth_type (result));
	lm_message_unref (result);

	result = lm_connection_send_with_reply_and_block (connection, m, error);
	if (!result) {
		return FALSE;
	}

	type = lm_message_get_sub_type (result);
	lm_message_unref (result);
	
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_RESULT:
		return TRUE;
		break;
	case LM_MESSAGE_SUB_TYPE_ERROR:
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
	connection->server = g_strdup (server);
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

	if (lm_message_node_get_attribute (message->node, "id")) {
		id = g_strdup (lm_message_node_get_attribute (message->node, 
							      "id"));
	} else {
		id = _lm_utils_generate_id ();
		lm_message_node_set_attributes (message->node, "id", id, NULL);
	}

 	g_source_remove (g_source_get_id (connection->incoming_source));
	g_source_unref (connection->incoming_source);

	lm_connection_send (connection, message, error);

	while (!reply) {
		const gchar *m_id;
		gint         n;

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
		
	connection->disconnect_cb = _lm_utils_new_callback (function, 
							    user_data,
							    notify);
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

