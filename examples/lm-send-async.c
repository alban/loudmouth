/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Description:
 * A little program that let you send jabber messages to another person.
 * 
 * Build instructions:
 * gcc -o lm-send-async `pkg-config --cflags --libs loudmouth-1.0` lm-send-async.c
 */

#include <stdlib.h>
#include <string.h>

#include <loudmouth/loudmouth.h>

static GMainLoop *main_loop;
static gboolean   test_success = FALSE;

static gchar      expected_fingerprint[20];

static gchar     *server = NULL;
static gint       port = 5222;
static gchar     *username = NULL;
static gchar     *password = NULL;
static gchar     *resource = "lm-send-async";
static gchar     *recipient = NULL;
static gchar     *fingerprint = NULL;
static gchar     *message = "test asynchronous message";

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
    "Resource connect with [default=lm-send-async]", NULL },
  { "recipient", 'R', 0, G_OPTION_ARG_STRING, &recipient, 
    "Recipient to send the message to (e.g. user@server.org)", NULL },
  { "fingerprint", 'f', 0, G_OPTION_ARG_STRING, &fingerprint, 
    "SSL Fingerprint to use", NULL },
  { "message", 'm', 0, G_OPTION_ARG_STRING, &message, 
    "Message to send to recipient [default=test message]", NULL },
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
	g_print ("LmSendAsync: SSL status:%d\n", status);

	switch (status) {
	case LM_SSL_STATUS_NO_CERT_FOUND:
		g_printerr ("LmSendAsync: No certificate found!\n");
		break;
	case LM_SSL_STATUS_UNTRUSTED_CERT:
		g_printerr ("LmSendAsync: Certificate is not trusted!\n"); 
		break;
	case LM_SSL_STATUS_CERT_EXPIRED:
		g_printerr ("LmSendAsync: Certificate has expired!\n"); 
		break;
	case LM_SSL_STATUS_CERT_NOT_ACTIVATED:
		g_printerr ("LmSendAsync: Certificate has not been activated!\n"); 
		break;
	case LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH:
		g_printerr ("LmSendAsync: Certificate hostname does not match expected hostname!\n"); 
		break;
	case LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH: {
		const char *fpr = lm_ssl_get_fingerprint (ssl);
		g_printerr ("LmSendAsync: Certificate fingerprint does not match expected fingerprint!\n"); 
		g_printerr ("LmSendAsync: Remote fingerprint: ");
		print_finger (fpr, 16);

		g_printerr ("\n"
			    "LmSendAsync: Expected fingerprint: ");
		print_finger (expected_fingerprint, 16);
		g_printerr ("\n");
		break;
	}
	case LM_SSL_STATUS_GENERIC_ERROR:
		g_printerr ("LmSendAsync: Generic SSL error!\n"); 
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
		GError    *error = NULL;
		LmMessage *m;

		g_print ("LmSendAsync: Authenticated successfully\n");
		
		m = lm_message_new (recipient, LM_MESSAGE_TYPE_MESSAGE);
		lm_message_node_add_child (m->node, "body", message);
		
		if (!lm_connection_send (connection, m, &error)) {
			g_printerr ("LmSendAsync: Failed to send message:'%s'\n", 
				 lm_message_node_to_string (m->node));
		} else {
			g_print ("LmSendAsync: Sent message:'%s'\n", 
				 lm_message_node_to_string (m->node));
			test_success = TRUE;
		}
		
		lm_message_unref (m);
	} else {
		g_printerr ("LmSendAsync: Failed to authenticate\n");
	}

	lm_connection_close (connection, NULL);
	g_main_loop_quit (main_loop);
}

static void
connection_open_cb (LmConnection *connection,
		    gboolean      success,
		    gpointer      user_data)
{
	if (success) {
		gchar *user;

		user = get_part_name (username);
		if (!lm_connection_authenticate (connection, user, 
						 password, resource,
						 connection_auth_cb, 
						 NULL, FALSE,  NULL)) {
			g_free (user);
			g_printerr ("LmSendAsync: Failed to send authentication\n");
			g_main_loop_quit (main_loop);
			return;
		}

		g_free (user);

		g_print ("LmSendAsync: Sent authentication message\n");
	} else {
		g_printerr ("LmSendAsync: Failed to connect\n");
		g_main_loop_quit (main_loop);
	}
}

int
main (int argc, char **argv)
{
	GMainContext   *main_context;
	GOptionContext *context;
        LmConnection   *connection;

	context = g_option_context_new ("- test send message asynchronously");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);
	
	if (!username || !password || !recipient) {
		g_printerr ("For usage, try %s --help\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (username && strchr (username, '@') == NULL) {
		g_printerr ("LmSendAsync: Username must have an '@' included\n");
		return EXIT_FAILURE;
	}

	main_context = g_main_context_new ();
        connection = lm_connection_new_with_context (server, main_context);
	lm_connection_set_port (connection, port);
	lm_connection_set_jid (connection, username);

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

	if (!lm_connection_open (connection, 
                                 (LmResultFunction) connection_open_cb,
                                 NULL, NULL, NULL)) {
                g_printerr ("LmSendAsync: Could not open a connection\n");
		return EXIT_FAILURE;
        }

        main_loop = g_main_loop_new (main_context, FALSE);
        g_main_loop_run (main_loop);

	return (test_success ? EXIT_SUCCESS : EXIT_FAILURE);
}
