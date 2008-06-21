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

/**
 * LmMessageType:
 * @LM_MESSAGE_TYPE_MESSAGE: a message, <message/>
 * @LM_MESSAGE_TYPE_PRESENCE: a presence element, <presence/>
 * @LM_MESSAGE_TYPE_IQ: an info/query element, <iq/>
 * @LM_MESSAGE_TYPE_STREAM: the stream:stream element, you probably don't need to create a message of this type.
 * @LM_MESSAGE_TYPE_STREAM_ERROR: a stream:error element
 * @LM_MESSAGE_TYPE_STREAM_FEATURES:  * @LM_MESSAGE_TYPE_AUTH:  * @LM_MESSAGE_TYPE_CHALLENGE:  * @LM_MESSAGE_TYPE_RESPONSE:  * @LM_MESSAGE_TYPE_SUCCESS:  * @LM_MESSAGE_TYPE_FAILURE:  * @LM_MESSAGE_TYPE_PROCEED:  * @LM_MESSAGE_TYPE_STARTTLS:  * @LM_MESSAGE_TYPE_UNKNOWN: incoming message is of some unknown type.
 * 
 * Describes what type of message a message is. This maps directly to top level elements in the jabber protocol.
 */
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

/**
 * LmMessageSubType:
 * @LM_MESSAGE_SUB_TYPE_NOT_SET: the default. No "type" attribute will be sent.
 * @LM_MESSAGE_SUB_TYPE_AVAILABLE: presence is available, applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_NORMAL:  * @LM_MESSAGE_SUB_TYPE_CHAT: message is a chat message, applies to message type "message"
 * @LM_MESSAGE_SUB_TYPE_GROUPCHAT: message is a group chat message, applies to message type "message"
 * @LM_MESSAGE_SUB_TYPE_HEADLINE: message is a headline message, applies to message type "message"
 * @LM_MESSAGE_SUB_TYPE_UNAVAILABLE: presence is unavailable, applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_PROBE: a probe presence, applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_SUBSCRIBE: try to subscribe to another jids presence, applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE: unsubscribes from another jids presence, applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_SUBSCRIBED: reply from a subscribe message, informs that the subscription was successful. Applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED: reply from subscribe or unsubscribe message. If it's a reply from a subscribe message it notifies that the subscription failed. Applies to message type "presence"
 * @LM_MESSAGE_SUB_TYPE_GET: used to get information from an IQ query, applies to message type "iq"
 * @LM_MESSAGE_SUB_TYPE_SET: used to set information in a IQ call, applised to message type "iq"
 * @LM_MESSAGE_SUB_TYPE_RESULT: message is an IQ reply, applies to message type "iq"
 * @LM_MESSAGE_SUB_TYPE_ERROR: messages is an error, applies to all message types.
 * 
 * Describes the sub type of a message. This is equal to the "type" attribute in the jabber protocol. What sub type a message can have is depending on the type of the message.
 */
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
