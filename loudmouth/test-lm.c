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

#include <glib.h>
#include <string.h>
#include <loudmouth/loudmouth.h>
#ifdef __WIN32__
#include <winsock2.h>
#endif
#include "lm-internals.h"

#define USAGE "Usage: test-lm <server> <username> <password>\n"

typedef struct {
	gchar *name;
	gchar *passwd;
} UserInfo;

static void
authentication_cb (LmConnection *connection, gboolean result, gpointer ud)
{
	g_print ("Auth: %d\n", result);

	if (result == TRUE) {
		LmMessage *m;
		
		m = lm_message_new_with_sub_type (NULL,
						  LM_MESSAGE_TYPE_PRESENCE,
						  LM_MESSAGE_SUB_TYPE_AVAILABLE);
		g_print (":: %s\n", lm_message_node_to_string (m->node));
		
		lm_connection_send (connection, m, NULL);
		lm_message_unref (m);
	}
}

static void
connection_open_cb (LmConnection *connection, gboolean result, UserInfo *info)
{
	g_print ("Connected callback\n");
	lm_connection_authenticate (connection,
				    info->name, info->passwd, "TestLM",
				    authentication_cb, NULL,FALSE,  NULL);
	g_print ("Sent auth message\n");
}

static LmHandlerResult
handle_messages (LmMessageHandler *handler,
		 LmConnection     *connection,
		 LmMessage        *m,
		 gpointer          user_data)
{
	g_print ("Incoming message from: %s\n",
		 lm_message_node_get_attribute (m->node, "from"));

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

int 
main (int argc, char **argv)
{
	GMainLoop        *main_loop;
	LmConnection     *connection;
	LmMessageHandler *handler;
	gboolean          result;
	UserInfo         *info;
#ifdef __WIN32__
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
#endif
	
	if (argc < 4) {
		g_print (USAGE);
		return 1;
	}

/* Needed to build on Win32. */
#ifdef __WIN32__
	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return;
	}

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

	if ( LOBYTE( wsaData.wVersion ) != 2 ||
			HIBYTE( wsaData.wVersion ) != 2 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		WSACleanup( );
		return;
	}
#endif

        connection = lm_connection_new (argv[1]);

	/*
	if (lm_connection_supports_ssl ()) {
		lm_connection_set_port (connection, 5223);
		lm_connection_set_use_ssl (connection, TRUE);
	}
	*/

	handler = lm_message_handler_new (handle_messages, NULL, NULL);
	lm_connection_register_message_handler (connection, handler, 
						LM_MESSAGE_TYPE_MESSAGE, 
						LM_HANDLER_PRIORITY_NORMAL);
	
	lm_message_handler_unref (handler);
	
	info = g_new0 (UserInfo, 1);
	info->name = g_strdup (argv[2]);
	info->passwd = g_strdup (argv[3]);
	
	result = lm_connection_open (connection,
				     (LmResultFunction) connection_open_cb,
				     info, NULL, NULL);

	if (!result) {
		g_print ("Opening connection failed: %d\n", result);
	} else {
		g_print ("Returned from the connection_open\n");
	}
	
	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}
