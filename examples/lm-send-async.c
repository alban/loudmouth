/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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

typedef struct {
        const gchar *recipient;
        const gchar *message;
} MessageData;

typedef struct {
        const gchar *username;
        const gchar *password;
        const gchar *resource;
        MessageData *msg_data;
} ConnectData;

static GMainLoop *main_loop;

static void
print_usage (const gchar *exec_name) 
{
        g_print ("Usage: %s -s <server> -u <username> -p <password> -m <message> -t <recipient> [--port <port>] [-r <resource>]\n", exec_name);
}

static void
connection_auth_cb (LmConnection *connection, 
                    gboolean      success, 
                    MessageData  *data)
{
        LmMessage *m;
        gboolean   result;
        GError    *error = NULL;

        if (!success) {
                g_error ("Authentication failed");
        }
        
        m = lm_message_new (data->recipient, LM_MESSAGE_TYPE_MESSAGE);
        lm_message_node_add_child (m->node, "body", data->message);
        
        result = lm_connection_send (connection, m, &error);
        lm_message_unref (m);
        
        if (!result) {
                g_error ("lm_connection_send failed");
        }

        lm_connection_close (connection, NULL);
        
        g_main_loop_quit (main_loop);
}

static void
connection_open_result_cb (LmConnection *connection,
                           gboolean      success,
                           ConnectData  *data)
{
        GError *error = NULL;
        
        if (!success) {
                g_error ("Connection failed");
        }

        if (!lm_connection_authenticate (connection, data->username,
                                         data->password, data->resource,
                                         (LmResultFunction) connection_auth_cb,
                                         data->msg_data,
                                         g_free,
                                         &error)) {
                g_error ("lm_connection_authenticate failed");
        }
}

int
main (int argc, char **argv)
{
        LmConnection *connection;
	GMainContext *context;
        const gchar  *server = NULL;
        const gchar  *resource = "jabber-send";
        const gchar  *recipient = NULL;
        const gchar  *message = NULL;
        const gchar  *username = NULL;
        const gchar  *password = NULL;
        guint         port = LM_CONNECTION_DEFAULT_PORT;
        MessageData  *msg_data;
        ConnectData  *connect_data;
        GError       *error = NULL;
        gint          i;

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
        
	context = g_main_context_new ();
        connection = lm_connection_new_with_context (server, context);

        msg_data = g_new0 (MessageData, 1);
        msg_data->recipient = recipient;
        msg_data->message = message;
       
        connect_data = g_new0 (ConnectData, 1);
        connect_data->username = username;
        connect_data->password = password;
        connect_data->resource = resource;
        connect_data->msg_data = msg_data;
        
        if (!lm_connection_open (connection, 
                                 (LmResultFunction) connection_open_result_cb,
                                 connect_data,
                                 g_free,
                                 &error)) {
                g_error ("lm_connection_open failed");
        }

        main_loop = g_main_loop_new (context, FALSE);
        g_main_loop_run (main_loop);

        return 0;
}
