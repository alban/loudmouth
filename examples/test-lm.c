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
#include <stdlib.h>

#include <glib.h>
#include <loudmouth/loudmouth.h>

static GMainLoop *main_loop = NULL;
static gboolean   test_success = FALSE;

static gchar      expected_fingerprint[20];

static gchar     *server = NULL;
static gint       port = 5222;
static gchar     *username = NULL;
static gchar     *password = NULL;
static gchar     *resource = "test-lm";
static gchar     *fingerprint = NULL;

static GOptionEntry entries[] = 
{
  { "server", 's', 0, G_OPTION_ARG_STRING, &server, 
    "Server to connect to", NULL },
  { "port", 'P', 0, G_OPTION_ARG_INT, &port, 
    "Port to connect to [default=5222]", NULL },
  { "username", 'u', 0, G_OPTION_ARG_STRING, &username, 
    "Username to connect with (user@server.org)", NULL },
  { "password", 'p', 0, G_OPTION_ARG_STRING, &password, 
    "Password to try", NULL },
  { "resource", 'r', 0, G_OPTION_ARG_STRING, &resource, 
    "Resource connect with [default=test-lm]", NULL },
  { "fingerprint", 'f', 0, G_OPTION_ARG_STRING, &fingerprint, 
    "SSL Fingerprint to use", NULL },
  { NULL }
};

static gchar *
get_part_name (const gchar *username)
{
	const gchar *ch;

	g_return_val_if_fail (username != NULL, NULL);

	ch = strchr (username, '@');
	if (!ch) {
		return NULL;
	}

	return g_strndup (username, ch - username);
}

static void
print_finger (const char   *fpr,
	      unsigned int  size)
{
	gint i;
	for (i = 0; i < size-1; i++) {
		g_printerr ("%02X:", fpr[i]);
	}
	
	g_printerr ("%02X", fpr[size-1]);
}

static LmSSLResponse
ssl_cb (LmSSL       *ssl, 
	LmSSLStatus  status, 
	gpointer     ud)
{
	g_print ("TestLM: SSL status:%d\n", status);

	switch (status) {
	case LM_SSL_STATUS_NO_CERT_FOUND:
		g_printerr ("TestLM: No certificate found!\n");
		break;
	case LM_SSL_STATUS_UNTRUSTED_CERT:
		g_printerr ("TestLM: Certificate is not trusted!\n"); 
		break;
	case LM_SSL_STATUS_CERT_EXPIRED:
		g_printerr ("TestLM: Certificate has expired!\n"); 
		break;
	case LM_SSL_STATUS_CERT_NOT_ACTIVATED:
		g_printerr ("TestLM: Certificate has not been activated!\n"); 
		break;
	case LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH:
		g_printerr ("TestLM: Certificate hostname does not match expected hostname!\n"); 
		break;
	case LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH: {
		const char *fpr = lm_ssl_get_fingerprint (ssl);
		g_printerr ("TestLM: Certificate fingerprint does not match expected fingerprint!\n"); 
		g_printerr ("TestLM: Remote fingerprint: ");
		print_finger (fpr, 16);

		g_printerr ("\n"
			    "TestLM: Expected fingerprint: ");
		print_finger (expected_fingerprint, 16);
		g_printerr ("\n");
		break;
	}
	case LM_SSL_STATUS_GENERIC_ERROR:
		g_printerr ("TestLM: Generic SSL error!\n"); 
		break;
	}

	return LM_SSL_RESPONSE_CONTINUE;
}

static void
connection_auth_cb (LmConnection *connection,
		    gboolean      success, 
		    gpointer      user_data)
{
	if (success) {
		LmMessage *m;
		
		test_success = TRUE;
		g_print ("TestLM: Authenticated successfully\n");

		m = lm_message_new_with_sub_type (NULL,
						  LM_MESSAGE_TYPE_PRESENCE,
						  LM_MESSAGE_SUB_TYPE_AVAILABLE);
		lm_connection_send (connection, m, NULL);
		g_print ("TestLM: Sent presence message:'%s'\n", 
			 lm_message_node_to_string (m->node));

		lm_message_unref (m);
	} else {
		g_printerr ("TestLM: Failed to authenticate\n");
		g_main_loop_quit (main_loop);
	}
}

static void
connection_open_cb (LmConnection *connection, 
		    gboolean      success,
		    gpointer      user_data)
{
	if (success) {
		gchar *user;

		user = get_part_name (username);
		lm_connection_authenticate (connection, user, 
					    password, resource,
					    connection_auth_cb, 
					    NULL, FALSE,  NULL);
		g_free (user);
		
		g_print ("TestLM: Sent authentication message\n");
	} else {
		g_printerr ("TestLM: Failed to connect\n");
		g_main_loop_quit (main_loop);
	}
}

static void
connection_close_cb (LmConnection       *connection, 
		     LmDisconnectReason  reason,
		     gpointer            user_data)
{
	const char *str;
	
	switch (reason) {
	case LM_DISCONNECT_REASON_OK:
		str = "LM_DISCONNECT_REASON_OK";
		break;
	case LM_DISCONNECT_REASON_PING_TIME_OUT:
		str = "LM_DISCONNECT_REASON_PING_TIME_OUT";
		break;
	case LM_DISCONNECT_REASON_HUP:
		str = "LM_DISCONNECT_REASON_HUP";
		break;
	case LM_DISCONNECT_REASON_ERROR:
		str = "LM_DISCONNECT_REASON_ERROR";
		break;
	case LM_DISCONNECT_REASON_UNKNOWN:
	default:
		str = "LM_DISCONNECT_REASON_UNKNOWN";
		break;
	}

	g_print ("TestLM: Disconnected, reason:%d->'%s'\n", reason, str);
}

static LmHandlerResult
handle_messages (LmMessageHandler *handler,
		 LmConnection     *connection,
		 LmMessage        *m,
		 gpointer          user_data)
{
	g_print ("TestLM: Incoming message from: %s\n",
		 lm_message_node_get_attribute (m->node, "from"));

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

int 
main (int argc, char **argv)
{
	GOptionContext   *context;
	LmConnection     *connection;
	LmMessageHandler *handler;
	gboolean          result;
	GError           *error = NULL;
	
	context = g_option_context_new ("- test Loudmouth");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);
	
	if (!server || !username || !password) {
		g_printerr ("For usage, try %s --help\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (fingerprint && !lm_ssl_is_supported ()) {
		g_printerr ("TestLM: SSL is not supported in this build\n");
		return EXIT_FAILURE;
	}

	if (username && strchr (username, '@') == NULL) {
		g_printerr ("TestLM: Username must have an '@' included\n");
		return EXIT_FAILURE;
	}

        connection = lm_connection_new (server);
	lm_connection_set_port (connection, port);
	lm_connection_set_jid (connection, username);

	handler = lm_message_handler_new (handle_messages, NULL, NULL);
	lm_connection_register_message_handler (connection, handler, 
						LM_MESSAGE_TYPE_MESSAGE, 
						LM_HANDLER_PRIORITY_NORMAL);
	
	lm_message_handler_unref (handler);
	
	lm_connection_set_disconnect_function (connection,
					       connection_close_cb,
					       NULL, NULL);

	if (fingerprint) {
		LmSSL *ssl;
		char  *p;
		int    i;
		
		if (port == LM_CONNECTION_DEFAULT_PORT) {
			lm_connection_set_port (connection,
						LM_CONNECTION_DEFAULT_PORT_SSL);
		}

		for (i = 0, p = fingerprint; *p && *(p+1); i++, p += 3) {
			expected_fingerprint[i] = (unsigned char) g_ascii_strtoull (p, NULL, 16);
		}
	
		ssl = lm_ssl_new (expected_fingerprint,
				  (LmSSLFunction) ssl_cb,
				  NULL, NULL);

                lm_ssl_use_starttls (ssl, TRUE, FALSE);
	
		lm_connection_set_ssl (connection, ssl);
		lm_ssl_unref (ssl);
	}

	result = lm_connection_open (connection,
				     (LmResultFunction) connection_open_cb,
				     NULL, NULL, &error);

	if (!result) {
		g_printerr ("TestLM: Opening connection failed, error:%d->'%s'\n", 
			 error->code, error->message);
		g_free (error);
		return EXIT_FAILURE;
	}
	
	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return (test_success ? EXIT_SUCCESS : EXIT_FAILURE);
}
