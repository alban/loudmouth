/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

LmCallback *_lm_utils_new_callback (gpointer func, 
				    gpointer user_data,
				    GDestroyNotify notify);
void _lm_utils_free_callback (LmCallback *cb);

gchar *            _lm_utils_generate_id             (void);
LmHandlerResult _lm_message_handler_handle_message   (LmMessageHandler *handler,
						      LmConnection     *connection,
						      LmMessage        *messag);

const gchar *      _lm_message_type_to_string         (LmMessageType     type);
const gchar *      _lm_message_sub_type_to_string     (LmMessageSubType  type);
LmMessage *        _lm_message_new_from_node          (LmMessageNode    *node);
void               _lm_message_node_add_child_node    (LmMessageNode    *node,
						       LmMessageNode    *child);
LmMessageNode *    _lm_message_node_new               (const gchar  *name);
void               _lm_debug_init                     (void);

#endif /* __LM_INTERNALS_H__ */
