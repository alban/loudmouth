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

static void
print_usage (const gchar *exec_name)
{
	g_print ("Usage: %s <server> <username> <oldpassword> <newpassword>\n",
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
	GError        *error = NULL;
	LmMessage     *m;
	LmMessageNode *query;

	if (argc < 5) {
		print_usage (argv[0]);
		return -1;
	}

	server = argv[1];
	username = argv[2];
	old_pass = argv[3];
	new_pass = argv[4];

	connection = lm_connection_new (server);

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
	
	if (!lm_connection_send (connection, m, &error)) {
		g_error ("Failed to send: %s\n", error->message);
	}
	
	lm_connection_close (connection, NULL);

	return 0;
}


