/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
 * Copyright (C) 2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003 CodeFactory AB. 
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

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#ifndef __WIN32__
  #include <netdb.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
#else
  #include <winsock2.h>
#endif

#include "lm-debug.h"
#include "lm-error.h"
#include "lm-internals.h"
#include "lm-parser.h"
#include "lm-sha.h"
#include "lm-queue.h"
#include "lm-connection.h"

#define IN_BUFFER_SIZE 1024

typedef struct {
	LmHandlerPriority  priority;
	LmMessageHandler  *handler;
} HandlerData;

typedef struct {
	GSource source;
	
	LmConnection *connection;
} LmIncomingSource;

struct _LmConnection {
	/* Parameters */
	gchar          *server;
	guint           port;
	gboolean        use_ssl;

#ifdef HAVE_GNUTLS
	gnutls_session  gnutls_session;
	gnutls_certificate_client_credentials gnutls_xcred;
#endif
	
	gboolean    is_open;
	gboolean    is_authenticated;
	
	LmParser   *parser;
	gchar      *stream_id;

	GHashTable *id_handlers;
	GSList     *handlers[LM_MESSAGE_TYPE_UNKNOWN];

	/* Communication */
	GIOChannel *io_channel;
	guint       io_watch_in;
	guint       io_watch_err;
	guint       io_watch_hup;

	LmCallback *open_cb;
	LmCallback *close_cb;
	LmCallback *auth_cb;
	LmCallback *register_cb;

	LmCallback *disconnect_cb;

	LmQueue    *incoming_messages;
	GSource    *incoming_source;

	gint        ref_count;
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
static gboolean connection_do_open         (LmConnection     *connection,
					      GError          **error);

static void     connection_do_close           (LmConnection          *connection);
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
static GSource * connection_create_source    (LmConnection *connection);
static void      connection_signal_disconnect (LmConnection *connection,
					       LmDisconnectReason reason);

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
	
	if (connection->is_open) {
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
	lm_message_ref (m);

	lm_verbose ("New message with type=\"%s\" from: %s\n",
		    _lm_message_type_to_string (lm_message_get_type (m)),
		    lm_message_node_get_attribute (m->node, "from"));

	lm_queue_push_tail (connection->incoming_messages, m);
}

static gboolean
connection_do_open (LmConnection *connection, GError **error)
{
	gint             fd = -1;
	int              err = -1;
	struct addrinfo  req;
	struct addrinfo *ans;
	struct addrinfo *tmpaddr;
	char             name[NI_MAXHOST];
	char             portname[NI_MAXSERV];
	
	g_return_val_if_fail (connection != NULL, FALSE);
	
	memset (&req, 0, sizeof(req));

	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;

	g_log (LM_LOG_DOMAIN,LM_LOG_LEVEL_NET,
	       "Going to connect to %s\n",connection->server);
	
	if ((err = getaddrinfo (connection->server, NULL, &req, &ans)) != 0) {
		g_set_error (error,
			     LM_ERROR,                 
			     LM_ERROR_CONNECTION_OPEN,   
			     "getaddrinfo() failed");
		return FALSE;
	}

#ifdef HAVE_GNUTLS
	if (connection->use_ssl) {
		gnutls_global_init ();
		gnutls_certificate_allocate_credentials (&connection->gnutls_xcred);
	}
#endif

	for (tmpaddr = ans ; tmpaddr != NULL ; tmpaddr = tmpaddr->ai_next) {
		((struct sockaddr_in *) tmpaddr->ai_addr)->sin_port = htons (connection->port);

		getnameinfo (tmpaddr->ai_addr,
			     tmpaddr->ai_addrlen,
			     name,     sizeof (name),
			     portname, sizeof (portname),
			     NI_NUMERICHOST | NI_NUMERICSERV);

		g_log (LM_LOG_DOMAIN,LM_LOG_LEVEL_NET,
		      "Trying %s port %s...\n", name, portname);

		fd = socket (tmpaddr->ai_family, 
			     tmpaddr->ai_socktype, 
			     tmpaddr->ai_protocol);
		if (fd < 0) {
			continue;
		}
		
		err = connect (fd,tmpaddr->ai_addr, tmpaddr->ai_addrlen);
		if (err == 0) {
			/* connection successfull */
			break;
		}
		
		close (fd);
	}
	
	freeaddrinfo (ans);

	/* check for failure */
	if (fd < 0 || err < 0) {
		g_set_error (error,
			     LM_ERROR,
			     LM_ERROR_CONNECTION_OPEN,
			     "connection failed");
		return FALSE;
	}

#ifdef HAVE_GNUTLS
	if (connection->use_ssl) {
		int ret;
		const int cert_type_priority[2] =
		{ GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP };

		gnutls_init (&connection->gnutls_session, GNUTLS_CLIENT);
		gnutls_set_default_priority (connection->gnutls_session);
		gnutls_certificate_type_set_priority (connection->gnutls_session,
						      cert_type_priority);
		gnutls_credentials_set (connection->gnutls_session,
					GNUTLS_CRD_CERTIFICATE,
					connection->gnutls_xcred);
		
		gnutls_transport_set_ptr (connection->gnutls_session, 
					  (gnutls_transport_ptr) fd);

		ret = gnutls_handshake (connection->gnutls_session);
		
		if (ret < 0) {
			gnutls_perror (ret);
			shutdown (fd, SHUT_RDWR);
			close (fd);
			connection_do_close (connection);
			g_set_error (error, LM_ERROR, LM_ERROR_CONNECTION_OPEN,
				     "*** GNUTLS handshake failed");
			return FALSE;
		}
	}
#endif
	
	connection->io_channel = g_io_channel_unix_new (fd);
	g_io_channel_set_close_on_unref (connection->io_channel, TRUE);
	g_io_channel_set_encoding (connection->io_channel, NULL, NULL);
	
	g_io_channel_set_buffered (connection->io_channel, FALSE);
	g_io_channel_set_flags (connection->io_channel,
				G_IO_FLAG_NONBLOCK, NULL);
	connection->io_watch_in = g_io_add_watch (connection->io_channel,
						  G_IO_IN,
						  (GIOFunc) connection_in_event,
						  connection);
	connection->io_watch_err = g_io_add_watch (connection->io_channel, 
						   G_IO_ERR,
						   (GIOFunc) connection_error_event,
						   connection);
	connection->io_watch_hup = g_io_add_watch (connection->io_channel,
						   G_IO_HUP,
						   (GIOFunc) connection_hup_event,
						   connection);

	connection->is_open = TRUE;

	if (!connection_send (connection,
			      "<?xml version='1.0' encoding='UTF-8'?>", -1, 
			      error)) {
		return FALSE;
	}

	return TRUE;
}

static void
connection_do_close (LmConnection *connection)
{
	if (connection->io_channel) {
		g_source_remove (connection->io_watch_in);
		g_source_remove (connection->io_watch_err);
		g_source_remove (connection->io_watch_hup);

		g_io_channel_unref (connection->io_channel);
		connection->io_channel = NULL;
	} 

	if (!connection->is_open) {
		return;
	}

	connection->is_open = FALSE;

#ifdef HAVE_GNUTLS
	if (connection->use_ssl) {
		gnutls_deinit (connection->gnutls_session);
		gnutls_certificate_free_credentials (connection->gnutls_xcred);
		gnutls_global_deinit ();
	}
#endif
}


static gboolean
connection_in_event (GIOChannel   *source,
		     GIOCondition  condition,
		     LmConnection *connection)
{
	gchar     buf[IN_BUFFER_SIZE];
	gint      bytes_read;
	GIOStatus status;
       
	if (!connection->io_channel) {
		return FALSE;
	}

#ifdef HAVE_GNUTLS
	if (connection->use_ssl) {
		bytes_read = gnutls_record_recv (connection->gnutls_session,
						 buf,IN_BUFFER_SIZE - 1);
		if (bytes_read <= 0) {
			status = G_IO_STATUS_ERROR;
			
			//connection_error_event (connection->io_channel, 
			//			G_IO_HUP,
			//			connection);
		}
		else {
			status = G_IO_STATUS_NORMAL;
		}
	} else {
#endif
	    status = g_io_channel_read_chars (connection->io_channel,
					      buf, IN_BUFFER_SIZE - 1,
					      &bytes_read,
					      NULL);
#ifdef HAVE_GNUTLS
	}
#endif

	if (status != G_IO_STATUS_NORMAL) {
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
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET, "\nRECV:\n");
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
	
	if (!lm_connection_is_open (connection)) {
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
	
#ifdef HAVE_GNUTLS
	if (connection->use_ssl) {
		while ((bytes_written = gnutls_record_send (connection->gnutls_session, str, len)) < 0)
			if (bytes_written != GNUTLS_E_INTERRUPTED &&
			    bytes_written != GNUTLS_E_AGAIN)
			{
				connection_error_event (connection->io_channel, G_IO_HUP,
							connection);
			}
		    
	} else {
#endif
		g_io_channel_write_chars (connection->io_channel, str, len, 
					  &bytes_written, NULL);
#ifdef HAVE_GNUTLS
	}
#endif

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
	} 
	else if (strcmp (type, "error") == 0) {
		result = FALSE;
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
	
	/* Check to see if the stream is correctly set up */
	result = TRUE;

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
	
	return !lm_queue_is_empty (connection->incoming_messages);
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

	m = (LmMessage *) lm_queue_pop_head (connection->incoming_messages);
	
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
	
	connection->port              = LM_CONNECTION_DEFAULT_PORT;
	connection->use_ssl           = FALSE;
	connection->disconnect_cb     = NULL;
	connection->incoming_messages = lm_queue_new ();
	connection->incoming_source   = connection_create_source (connection);
	
	connection->id_handlers = g_hash_table_new_full (g_str_hash, 
							 g_str_equal,
							 g_free, 
							 (GDestroyNotify) lm_message_handler_unref);
	connection->ref_count         = 1;
	g_source_attach (connection->incoming_source, NULL);
	
	for (i = 0; i < LM_MESSAGE_TYPE_UNKNOWN; ++i) {
		connection->handlers[i] = NULL;
	}

	connection->parser = lm_parser_new 
		((LmParserMessageFunction) connection_new_message_cb, 
		 connection, NULL);

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
	LmMessage *m;
	gboolean   result;
	
	g_return_val_if_fail (connection != NULL, FALSE);
	
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

	connection->open_cb = _lm_utils_new_callback (function, user_data, notify);
	
	lm_verbose ("Connecting to: %s:%d\n", 
		    connection->server, connection->port);
	
	if (!connection_do_open (connection, error)) {
		return FALSE;
	}
	
	m = lm_message_new (connection->server, LM_MESSAGE_TYPE_STREAM);
	lm_message_node_set_attributes (m->node,
					"xmlns:stream", "http://etherx.jabber.org/streams",
					"xmlns", "jabber:client",
					NULL);
	
	lm_verbose ("Opening stream...");
	
	result = lm_connection_send (connection, m, error);
	lm_message_unref (m);
	
	return result;
}

/**
 * lm_connection_open_and_block:
 * @connection: an #LmConnection
 * @error: location to store error, or %NULL
 * 
 * Opens @connection and waits until the stream is setup. 
 * 
 * Return value: #TRUE if no errors where encountered during opening and stream setup successfully, #FALSE otherwise.
 **/
gboolean
lm_connection_open_and_block (LmConnection *connection, GError **error)
{
	LmMessage *m;
	gboolean   result;
	gboolean   finished = FALSE;
	gboolean   ret_val = FALSE;

	g_return_val_if_fail (connection != NULL, FALSE);
	
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

	lm_verbose ("(Block)Connecting to: %s:%d\n", 
		    connection->server, connection->port);
	
	if (!connection_do_open (connection, error)) {
		return FALSE;
	}
	
	m = lm_message_new (connection->server, LM_MESSAGE_TYPE_STREAM);
	lm_message_node_set_attributes (m->node,
					"xmlns:stream", "http://etherx.jabber.org/streams",
					"xmlns", "jabber:client",
					NULL);
	
	lm_verbose ("Sending stream: \n%s\n", 
		    lm_message_node_to_string (m->node));
	
	result = lm_connection_send (connection, m, error);
	lm_message_unref (m);

 	g_source_remove (g_source_get_id (connection->incoming_source));
	g_source_unref (connection->incoming_source);

	while (!finished) {
		gint n;
		
		g_main_context_iteration (NULL, TRUE);
		
		if (lm_queue_is_empty (connection->incoming_messages)) {
			continue;
		}

		for (n = 0; n < connection->incoming_messages->length; n++) {
			LmMessage *m;

			m = lm_queue_peek_nth (connection->incoming_messages, n);
			if (lm_message_get_type (m) == LM_MESSAGE_TYPE_STREAM) {
				connection->stream_id = 
					g_strdup (lm_message_node_get_attribute (m->node, "id"));
				ret_val = TRUE;
				finished = TRUE;
				lm_queue_remove_nth (connection->incoming_messages, n);
				break;
			}
		}
	}
	
	connection->incoming_source = connection_create_source (connection);
	g_source_attach (connection->incoming_source, NULL);

	return ret_val;
}

/**
 * lm_connection_close:
 * @connection: #LmConnection to close 
 * @error: location to store error, or %NULL
 * 
 * A synchronos call to close the connection. When returning the connection is considered to be closed and can be opened again with lm_connection_open().
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
		    connection->server,
		    connection->port);
	
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
 * Tries to authenticate a user against the server. The #LmResult in the result callback will tell if it succeeded or not. 
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
		return FALSE;
		break;
	default:
		g_assert_not_reached ();
		break;
	} 

	return FALSE;
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
	return connection->is_open;
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
	return connection->is_authenticated;
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
	return connection->server;
}

/**
 * lm_connection_set_server:
 * @connection: an #LmConnection
 * @server: Address of the server
 * 
 * Sets the server address to @connection. Notice that @connection can't be open while doing this.
 **/
void
lm_connection_set_server (LmConnection *connection, const gchar *server)
{
	if (lm_connection_is_open (connection)) {
		g_warning ("Can't change server address while connected");
		return;
	}
	
	if (connection->server) {
		g_free (connection->server);
	}
	
	connection->server = g_strdup (server);
}

/**
 * lm_connection_get_port:
 * @connection: an #LmConnection
 * 
 * Fetches the port tnat @connection is using.
 * 
 * Return value: 
 **/
guint
lm_connection_get_port (LmConnection *connection)
{
	return connection->port;
}

/**
 * lm_connection_set_port:
 * @connection: an #LmConnection
 * @port: server port
 * 
 * Sets the server port of that @connection will be using.
 **/
void
lm_connection_set_port (LmConnection *connection, guint port)
{
	if (lm_connection_is_open (connection)) {
		g_warning ("Can't change server port while connected");
		return;
	}
	
	connection->port = port;
}

/**
 * lm_connection_supports_ssl:
 *
 * Checks whether Loudmouth supports SSL or not
 *
 * Return value: #TRUE if this installation of Loudmouth supports SSL, otherwise returnes #FALSE.
 **/
gboolean
lm_connection_supports_ssl (void)
{
#ifdef HAVE_GNUTLS
	return TRUE;
#else
	return FALSE;
#endif
}

/**
 * lm_connection_get_use_ssl:
 * @connection: an #LmConnection
 * 
 * Fetches if @connection is using SSL or not
 * 
 * Return value: #TRUE if @connection is using SSL, #FALSE otherwise.
 **/
gboolean
lm_connection_get_use_ssl (LmConnection *connection)
{
	return connection->use_ssl;
}

/**
 * lm_connection_set_use_ssl:
 * @connection: an #LmConnection
 * @use_ssl: whether to use SSL or not.
 * 
 * Sets whether @connection should use SSL for encryping traffic to/from the server.
 **/
void
lm_connection_set_use_ssl (LmConnection *connection, gboolean use_ssl)
{
	if (lm_connection_is_open (connection)) {
		g_warning ("use_ssl can't be changed while connected");
		return;
	}

	connection->use_ssl = use_ssl;
}

/**
 * lm_connection_send: 
 * @connection: #LmConnection to send connection over.
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
 * Return value: 
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

		g_main_context_iteration (NULL, TRUE);
	
		if (lm_queue_is_empty (connection->incoming_messages)) {
			continue;
		}

		for (n = 0; n < connection->incoming_messages->length; n++) {
			LmMessage *m;

			m = lm_queue_peek_nth (connection->incoming_messages, n);

			m_id = lm_message_node_get_attribute (m->node, "id");
			
			if (m_id && strcmp (m_id, id) == 0) {
				reply = m;
				lm_queue_remove_nth (connection->incoming_messages, n);
				break;
			}
		}
	}

	g_free (id);
	connection->incoming_source = connection_create_source (connection);
	g_source_attach (connection->incoming_source, NULL);

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
 * @connection it will be fried and shouldn't be used again.
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

#if 0
void
lm_connection_register (LmConnection           *connection,
		    const gchar        *username,
		    const gchar        *password,
		    const gchar        *resource,
		    LmRegisterCallback  callback,
		    gpointer            user_data)
{
	LmElement    *element;
	LmNode       *q_node;
	gchar        *id;
	static gint   register_id = 0;
	
	g_return_if_fail (connection != NULL);
	g_return_if_fail (lm_connection_is_open (connection));
	
	/* Use lm:iq:register name space */

	element = lm_iq_new (LM_IQ_TYPE_SET);
	
	q_node = lm_node_new ("query");
	lm_node_set_attribute (q_node, "xmlns", JABBER_IQ_REGISTER);
	lm_node_add_child (q_node, "username", username);
	lm_node_add_child (q_node, "password", password);
	lm_node_add_child (q_node, "resource", resource);

	lm_element_add_child_node (element, q_node);
	
	id = g_strdup_printf ("register_%d", ++register_id);
	lm_element_set_id (element, id);
	
	lm_connection_send (connection, element, NULL);
	connection_add_callback (connection, id, callback, user_data);
	lm_element_unref (element);

	g_free (id);

	
}

#endif
