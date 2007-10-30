/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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

/* Private functions that are internal to the library */

#ifndef __LM_INTERNALS_H__
#define __LM_INTERNALS_H__

#include <glib.h>

#include <sys/types.h>

#include "lm-message.h"
#include "lm-message-handler.h"
#include "lm-message-node.h"
#include "lm-sock.h"
#include "lm-socket.h"

#ifndef G_OS_WIN32
typedef int LmSocketT;
#else  /* G_OS_WIN32 */
typedef SOCKET LmSocketT;
#endif /* G_OS_WIN32 */

typedef struct {
	gpointer       func;
	gpointer       user_data;
	GDestroyNotify notify;
} LmCallback;

typedef struct {
	LmConnection *connection;
	LmSocket *socket;

	/* struct to save resolved address */
	struct addrinfo *resolved_addrs;
	struct addrinfo *current_addr;
	LmSocketT         fd;
	GIOChannel      *io_channel;
} LmConnectData;

gboolean         _lm_socket_failed_with_error (LmConnectData *connect_data,
                                                   int error);
gboolean         _lm_socket_failed            (LmConnectData *connect_data);
void             _lm_socket_succeeded         (LmConnectData *connect_data);
gboolean _lm_connection_async_connect_waiting (LmConnection *connection);
void _lm_connection_set_async_connect_waiting (LmConnection *connection,
					       gboolean      waiting);

LmCallback *     _lm_utils_new_callback             (gpointer          func, 
						     gpointer          data,
						     GDestroyNotify    notify);
void             _lm_utils_free_callback            (LmCallback       *cb);

gchar *          _lm_utils_generate_id              (void);
gchar *          _lm_utils_base64_encode            (const gchar      *str);
gchar *          _lm_utils_hostname_to_punycode     (const gchar      *hostname);
const gchar *    _lm_message_type_to_string         (LmMessageType     type);
const gchar *    _lm_message_sub_type_to_string     (LmMessageSubType  type);
LmMessage *      _lm_message_new_from_node          (LmMessageNode    *node);
void             _lm_message_node_add_child_node    (LmMessageNode    *node,
						     LmMessageNode    *child);
LmMessageNode *  _lm_message_node_new               (const gchar      *name);
void             _lm_debug_init                     (void);


gboolean         _lm_proxy_connect_cb               (GIOChannel *source,
                                                     GIOCondition condition,
                                                     gpointer data);
LmHandlerResult    
_lm_message_handler_handle_message                (LmMessageHandler *handler,
						   LmConnection     *connection,
						   LmMessage        *messag);
gboolean         _lm_sock_library_init            (void);
void             _lm_sock_library_shutdown        (void);
void             _lm_sock_set_blocking            (LmSocketT               sock,
						   gboolean               block);
void             _lm_sock_shutdown                (LmSocketT               sock);
void             _lm_sock_close                   (LmSocketT               sock);
LmSocketT         _lm_sock_makesocket              (int                    af,
						   int                    type,
						   int                    protocol);
int              _lm_sock_connect                 (LmSocketT               sock,
						   const struct sockaddr *name,
						   int                    namelen);
gboolean         _lm_sock_is_blocking_error       (int                    err);
gboolean         _lm_sock_is_blocking_success     (int                    err);
int              _lm_sock_get_last_error          (void);
void             _lm_sock_get_error               (LmSocketT               sock, 
						   void                  *error, 
						   socklen_t             *len);
const gchar *    _lm_sock_get_error_str           (int                    err);
const gchar *    _lm_sock_addrinfo_get_error_str  (int                    err);
gchar       *    _lm_sock_get_local_host          (LmSocketT              sock);

#ifdef USE_TCP_KEEPALIVES
gboolean         _lm_sock_set_keepalive (LmSocketT sock, int delay);
#endif /* USE_TCP_KEEPALIVES */

#endif /* __LM_INTERNALS_H__ */
