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

#ifndef __LM_MESSAGE_H__
#define __LM_MESSAGE_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

#include <loudmouth/lm-message-node.h>

G_BEGIN_DECLS

typedef struct LmMessagePriv LmMessagePriv;

typedef struct {
	LmMessageNode *node;

	LmMessagePriv *priv;
} LmMessage;

typedef enum {
	LM_MESSAGE_TYPE_MESSAGE,
	LM_MESSAGE_TYPE_PRESENCE,
	LM_MESSAGE_TYPE_IQ,
	LM_MESSAGE_TYPE_STREAM,
	LM_MESSAGE_TYPE_STREAM_ERROR,
	LM_MESSAGE_TYPE_STREAM_FEATURES,
	LM_MESSAGE_TYPE_AUTH,
	LM_MESSAGE_TYPE_CHALLENGE,
	LM_MESSAGE_TYPE_RESPONSE,
	LM_MESSAGE_TYPE_SUCCESS,
	LM_MESSAGE_TYPE_FAILURE,
	LM_MESSAGE_TYPE_PROCEED,
	LM_MESSAGE_TYPE_STARTTLS,
	LM_MESSAGE_TYPE_UNKNOWN
} LmMessageType;

typedef enum {
        LM_MESSAGE_SUB_TYPE_NOT_SET = -10,
	LM_MESSAGE_SUB_TYPE_AVAILABLE = -1,
	LM_MESSAGE_SUB_TYPE_NORMAL = 0,
	LM_MESSAGE_SUB_TYPE_CHAT,
        LM_MESSAGE_SUB_TYPE_GROUPCHAT,
        LM_MESSAGE_SUB_TYPE_HEADLINE,
        LM_MESSAGE_SUB_TYPE_UNAVAILABLE,
        LM_MESSAGE_SUB_TYPE_PROBE,
        LM_MESSAGE_SUB_TYPE_SUBSCRIBE,
        LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE,
        LM_MESSAGE_SUB_TYPE_SUBSCRIBED,
        LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED,
	LM_MESSAGE_SUB_TYPE_GET,
	LM_MESSAGE_SUB_TYPE_SET,
	LM_MESSAGE_SUB_TYPE_RESULT,
	LM_MESSAGE_SUB_TYPE_ERROR
} LmMessageSubType;

LmMessage *      lm_message_new               (const gchar      *to,
					       LmMessageType     type);
LmMessage *      lm_message_new_with_sub_type (const gchar      *to,
					       LmMessageType     type,
					       LmMessageSubType  sub_type);
LmMessageType    lm_message_get_type          (LmMessage        *message);
LmMessageSubType lm_message_get_sub_type      (LmMessage        *message);
LmMessageNode *  lm_message_get_node          (LmMessage        *message);
LmMessage *      lm_message_ref               (LmMessage        *message);
void             lm_message_unref             (LmMessage        *message);

G_END_DECLS

#endif /* __LM_MESSAGE_H__ */
