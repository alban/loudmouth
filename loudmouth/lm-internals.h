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

#include "lm-message.h"
#include "lm-message-handler.h"
#include "lm-message-node.h"

typedef struct {
	gpointer       func;
	gpointer       user_data;
	GDestroyNotify notify;
} LmCallback;

typedef struct {
	LmConnection    *connection;

	/* struct to save resolved address */
	struct addrinfo *resolved_addrs;
	struct addrinfo *current_addr;
	int              fd;
	GIOChannel           *io_channel;
} LmConnectData;

void             _lm_connection_failed_with_error (LmConnectData *connect_data,
                                                   int error);
void             _lm_connection_failed            (LmConnectData *connect_data);
gboolean         _lm_connection_succeeded (LmConnectData *connect_data);
LmCallback *     _lm_utils_new_callback             (gpointer          func, 
						     gpointer          data,
						     GDestroyNotify    notify);
void             _lm_utils_free_callback            (LmCallback       *cb);

gchar *          _lm_utils_generate_id              (void);
gchar *          _lm_utils_base64_encode            (const gchar      *str);
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
void             _lm_ssl_initialize                 (LmSSL            *ssl);
gboolean         _lm_ssl_begin                      (LmSSL            *ssl,
						     gint              fd,
						     const gchar      *server,
						     GError          **error);
GIOStatus        _lm_ssl_read                       (LmSSL            *ssl,
						     gchar            *buf,
						     gint              len,
						     gsize             *bytes_read);
gboolean         _lm_ssl_send                       (LmSSL            *ssl,
						     const gchar      *str,
						     gint              len);
void             _lm_ssl_close                      (LmSSL            *ssl);

LmHandlerResult    
_lm_message_handler_handle_message                (LmMessageHandler *handler,
						   LmConnection     *connection,
						   LmMessage        *messag);


#endif /* __LM_INTERNALS_H__ */
