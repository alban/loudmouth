/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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
 * Small tool to change password on an account
 */

#include <loudmouth/loudmouth.h>
#include <string.h>

static LmSSLResponse 
ssl_func (LmSSL        *ssl,
	  LmSSLStatus   status,
	  gpointer      user_data)
{
	return LM_SSL_RESPONSE_CONTINUE;
}

static void
print_usage (const gchar *exec_name)
{
	g_print ("Usage: %s <server> <username> <oldpassword> <newpassword> [--ssl] [--host <host>]\n",
		 exec_name);
}

int 
main (int argc, char **argv)
{
	LmConnection  *connection;
	const gchar   *server;
	const gchar   *username;
	const gchar   *old_pass;
	const gchar   *new_pass;
	const gchar   *host;
	GError        *error = NULL;
	LmMessage     *m;
	LmMessage     *reply;
	LmMessageNode *query;
	gboolean       use_ssl = FALSE;
	
	
	if (argc < 5) {
		print_usage (argv[0]);
		return -1;
	}

	server = argv[1];
	username = argv[2];
	old_pass = argv[3];
	new_pass = argv[4];
	host = NULL;

	if (argc >= 5) {
		int i;

		for (i = 5; i < argc; ++i) {
			if (strcmp (argv[i], "-s") == 0 ||
			    strcmp (argv[i], "--ssl") == 0) {
				use_ssl = TRUE;
			}
			else if (strcmp (argv[i], "-h") == 0 ||
				 strcmp (argv[i], "--host") == 0) {
				if (++i >= argc) {
					print_usage (argv[0]);
					return -1;
				} 

				host = argv[i];
				g_print ("HOST: %s\n", host);
			}
		}
	}

	connection = lm_connection_new (server);

	if (host) {
		gchar *jid;

		jid = g_strdup_printf ("%s@%s", username, host);
		g_print ("Setting jid to %s\n", jid);
		lm_connection_set_jid (connection, jid);
		g_free (jid);
	}
	
	if (use_ssl) {
		LmSSL *ssl;

		if (!lm_ssl_is_supported ()) {
			g_print ("This loudmouth installation doesn't support SSL\n");
			return 1;
		}

		g_print ("Setting ssl\n");
		ssl = lm_ssl_new (NULL, ssl_func, NULL, NULL);
		lm_connection_set_ssl (connection, ssl);
		lm_ssl_unref (ssl);

		lm_connection_set_port (connection,
					LM_CONNECTION_DEFAULT_PORT_SSL);
	}

	if (!lm_connection_open_and_block (connection, &error)) {
		g_error ("Failed to open: %s\n", error->message);
	}

	if (!lm_connection_authenticate_and_block (connection,
						   username, old_pass, 
						   "Password changer",
						   &error)) {
		g_error ("Failed to authenticate: %s\n", error->message);
	}

	m = lm_message_new_with_sub_type (NULL, LM_MESSAGE_TYPE_IQ, 
					 LM_MESSAGE_SUB_TYPE_SET);
	
	query = lm_message_node_add_child (m->node, "query", NULL);
	
	lm_message_node_set_attributes (query, "xmlns", "jabber:iq:register",
					NULL);
	lm_message_node_add_child (query, "username", username);
	lm_message_node_add_child (query, "password", new_pass);

	reply = lm_connection_send_with_reply_and_block (connection, m, &error);
	if (!reply) {
		g_error ("Failed to change password: %s\n", error->message);
	}	

	if (lm_message_get_sub_type (reply) == LM_MESSAGE_SUB_TYPE_RESULT) {
		g_print ("Password changed\n");
	} else {
		g_print ("Failed to change password\n");
		/* If this wasn't only an example we should check error code
		 * here to tell the user why it failed */
	}
	
	lm_connection_close (connection, NULL);

	return 0;
}


