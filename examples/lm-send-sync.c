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
 * Description:
 * A little program that let you send jabber messages to another person.
 * 
 * Build instructions:
 * gcc -o lm-send-sync `pkg-config --cflags --libs loudmouth-1.0` lm-send-sync.c
 */

#include <loudmouth/loudmouth.h>
#include <string.h>
#include <stdlib.h>

static void
print_usage (const gchar *exec_name) 
{
        g_print ("Usage: %s -s <server> -u <username> -p <password> -m <message> -t <recipient> [--port <port>] [-r <resource>]\n", exec_name);
}

int
main (int argc, char **argv)
{
        LmConnection *connection;
        const gchar  *server = NULL;
        const gchar  *resource = "jabber-send";
        const gchar  *recipient = NULL;
        const gchar  *message = NULL;
        const gchar  *username = NULL;
        const gchar  *password = NULL;
        guint         port = LM_CONNECTION_DEFAULT_PORT;
        GError       *error = NULL;
        gint          i;
	LmMessage    *m;

        for (i = 1; i < argc - 1; ++i) {
                gboolean arg = FALSE;
                
                if (strcmp ("-s", argv[i]) == 0) {
                        server = argv[i+1];
                        arg = TRUE;
                }
                else if (strcmp ("--port", argv[i]) == 0) {
                        port = atoi (argv[i+1]);
                        arg = TRUE;
                }
                else if (strcmp ("-m", argv[i]) == 0) {
                        message = argv[i+1];
                        arg = TRUE;
                }
                else if (strcmp ("-r", argv[i]) == 0) {
                        resource = argv[i+1];
                        arg = TRUE;
                }
                else if (strcmp ("-t", argv[i]) == 0) {
                        recipient = argv[i+1];
                        arg = TRUE;
                }
                else if (strcmp ("-u", argv[i]) == 0) {
                        username = argv[i+1];
                        arg = TRUE;
                }
                else if (strcmp ("-p", argv[i]) == 0) {
                        password = argv[i+1];
                        arg = TRUE;
                }

                if (arg) {
                        ++i;
                }
        }

        if (!server || !message || !recipient || !username || !password) {
                print_usage (argv[0]);
                return -1;
        }
        
        connection = lm_connection_new (server);

        if (!lm_connection_open_and_block (connection, &error)) {
                g_error ("Failed to open: %s\n", error->message);
        }

	if (!lm_connection_authenticate_and_block (connection,
						   username, password, resource,
						   &error)) {
		g_error ("Failed to authenticate: %s\n", error->message);
	}
	
	m = lm_message_new (recipient, LM_MESSAGE_TYPE_MESSAGE);
	lm_message_node_add_child (m->node, "body", message);
	
	if (!lm_connection_send (connection, m, &error)) {
		g_error ("Send failed: %s\n", error->message);
	}

	lm_message_unref (m);

	lm_connection_close (connection, NULL);
	lm_connection_unref (connection);
	
        return 0;
}

