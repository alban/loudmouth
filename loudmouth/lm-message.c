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

#include <config.h>
#include <string.h>

#include "lm-internals.h"
#include "lm-message.h"

#define PRIV(o) ((LmMessage *)o)->priv

static struct TypeNames 
{
        LmMessageType  type;
        const gchar   *name;
} type_names[] = {
	{ LM_MESSAGE_TYPE_MESSAGE,       "message"       },
	{ LM_MESSAGE_TYPE_PRESENCE,      "presence"      },
	{ LM_MESSAGE_TYPE_IQ,            "iq"            },
	{ LM_MESSAGE_TYPE_STREAM,        "stream:stream" },
	{ LM_MESSAGE_TYPE_STREAM_ERROR,  "stream:error"  }
};

static struct SubTypeNames 
{
        LmMessageSubType  type;
        const gchar      *name;
} sub_type_names[] = {
	{ LM_MESSAGE_SUB_TYPE_NORMAL,          "normal"        },
        { LM_MESSAGE_SUB_TYPE_CHAT,            "chat"          },
	{ LM_MESSAGE_SUB_TYPE_GROUPCHAT,       "groupchat"     },
	{ LM_MESSAGE_SUB_TYPE_HEADLINE,        "headline"      },
	{ LM_MESSAGE_SUB_TYPE_UNAVAILABLE,     "unavailable"   },
        { LM_MESSAGE_SUB_TYPE_PROBE,           "probe"         },
	{ LM_MESSAGE_SUB_TYPE_SUBSCRIBE,       "subscribe"     },
	{ LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE,     "unsubscribe"   },
	{ LM_MESSAGE_SUB_TYPE_SUBSCRIBED,      "subscribed"    },
	{ LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED,    "unsubscribed"  },
	{ LM_MESSAGE_SUB_TYPE_GET,             "get"           },
	{ LM_MESSAGE_SUB_TYPE_SET,             "set"           },
	{ LM_MESSAGE_SUB_TYPE_RESULT,          "result"        }, 
	{ LM_MESSAGE_SUB_TYPE_ERROR,           "error"         }
};

struct LmMessagePriv {
	LmMessageType    type;
	LmMessageSubType sub_type;
	gint             ref_count;
};

static LmMessageType
message_type_from_string (const gchar *type_str)
{
        gint i;

        if (!type_str) {
                return LM_MESSAGE_TYPE_UNKNOWN;
        }

        for (i = LM_MESSAGE_TYPE_MESSAGE;
	     i <= LM_MESSAGE_TYPE_STREAM_ERROR;
	     ++i) {
                if (strcmp (type_str, type_names[i].name) == 0) {
                        return i;
                }
        }

        return LM_MESSAGE_TYPE_UNKNOWN;
}


const gchar *
_lm_message_type_to_string (LmMessageType type)
{
        if (type < LM_MESSAGE_TYPE_MESSAGE ||
            type > LM_MESSAGE_TYPE_STREAM_ERROR) {
                type = LM_MESSAGE_TYPE_UNKNOWN;
        }

        return type_names[type].name;
}

static LmMessageSubType
message_sub_type_from_string (const gchar *type_str)
{
        gint i;

        if (!type_str) {
                return LM_MESSAGE_SUB_TYPE_NOT_SET;
        }

        for (i = LM_MESSAGE_SUB_TYPE_NORMAL;
	     i <= LM_MESSAGE_SUB_TYPE_ERROR;
	     ++i) {
                if (g_ascii_strcasecmp (type_str, 
					sub_type_names[i].name) == 0) {
                        return i;
                }
        }

        return LM_MESSAGE_SUB_TYPE_NOT_SET;
}

const gchar *
_lm_message_sub_type_to_string (LmMessageSubType type)
{
        if (type < LM_MESSAGE_SUB_TYPE_NORMAL ||
            type > LM_MESSAGE_SUB_TYPE_ERROR) {
		return NULL;
        }

        return sub_type_names[type].name;
}

static LmMessageSubType
message_sub_type_when_unset (LmMessageType type) {
	LmMessageSubType sub_type = LM_MESSAGE_SUB_TYPE_NORMAL;

	switch (type) {
	case LM_MESSAGE_TYPE_MESSAGE:
		/* A message without type should be handled like a message with
		 * type=normal, but we won't set it to that since then the user
		 * will not know if it's set or not.
		 */
		sub_type = LM_MESSAGE_SUB_TYPE_NOT_SET;
		break;
	case LM_MESSAGE_TYPE_PRESENCE:
		sub_type = LM_MESSAGE_SUB_TYPE_AVAILABLE;
		break;
	case LM_MESSAGE_TYPE_IQ:
		sub_type = LM_MESSAGE_SUB_TYPE_GET;
		break;
	default:
		break;
	}

	return sub_type;
}

LmMessage *
_lm_message_new_from_node (LmMessageNode *node)
{
	LmMessage        *m;
	LmMessageType     type;
	LmMessageSubType  sub_type;
	const gchar      *sub_type_str;
	
	type = message_type_from_string (node->name);

	if (type == LM_MESSAGE_TYPE_UNKNOWN) {
		return NULL;
	}

	sub_type_str = lm_message_node_get_attribute (node, "type");
	if (sub_type_str) {
		sub_type = message_sub_type_from_string (sub_type_str);
	} else {
		sub_type = message_sub_type_when_unset (type);
	}

	m = g_new0 (LmMessage, 1);
	m->priv = g_new0 (LmMessagePriv, 1);
	
	PRIV(m)->ref_count = 1;
	PRIV(m)->type = type;
	PRIV(m)->sub_type = sub_type;
	
	m->node = lm_message_node_ref (node);
	
	return m;
}

/**
 * lm_message_new:
 * @to: receipient jid
 * @type: message type
 * 
 * Creates a new #LmMessage which can be sent with lm_connection_send() or 
 * lm_connection_send_with_reply(). If @to is %NULL the message is sent to the
 * server. The returned message should be unreferenced with lm_message_unref() 
 * when caller is finished with it.
 * 
 * Return value: a newly created #LmMessage
 **/
LmMessage *
lm_message_new (const gchar *to, LmMessageType type)
{
	LmMessage *m;
	gchar     *id;

	m       = g_new0 (LmMessage, 1);
	m->priv = g_new0 (LmMessagePriv, 1);

	PRIV(m)->ref_count = 1;
	PRIV(m)->type      = type;
	PRIV(m)->sub_type  = message_sub_type_when_unset (type);
	
	m->node = _lm_message_node_new (_lm_message_type_to_string (type));

	id = _lm_utils_generate_id ();
	lm_message_node_set_attribute (m->node, "id", id);
	g_free (id);
	
	if (to) {
		lm_message_node_set_attribute (m->node, "to", to);
	}

	if (type == LM_MESSAGE_TYPE_IQ) {
		lm_message_node_set_attribute (m->node, "type", "get");
	}
	
	return m;
}

/**
 * lm_message_new_with_sub_type:
 * @to: receipient jid
 * @type: message type
 * @sub_type: message sub type
 * 
 * Creates a new #LmMessage with sub type set. See lm_message_new() for more 
 * information.
 * 
 * Return value: a newly created #LmMessage
 **/
LmMessage *
lm_message_new_with_sub_type (const gchar      *to,
			      LmMessageType     type, 
			      LmMessageSubType  sub_type)
{
	LmMessage   *m;
	const gchar *type_str;

	m = lm_message_new (to, type);

	type_str = _lm_message_sub_type_to_string (sub_type);

	if (type_str) {
		lm_message_node_set_attributes (m->node,
						"type", type_str, NULL);
		PRIV(m)->sub_type = sub_type;
	}

	return m;
}

/**
 * lm_message_get_type:
 * @message: an #LmMessage
 * 
 * Fetches the type of @message.
 * 
 * Return value: the message type
 **/
LmMessageType
lm_message_get_type (LmMessage *message)
{
	g_return_val_if_fail (message != NULL, LM_MESSAGE_TYPE_UNKNOWN);
	
	return PRIV(message)->type;
}

/**
 * lm_message_get_sub_type:
 * @message: 
 * 
 * Fetches the sub type of @message.
 * 
 * Return value: the message sub type
 **/
LmMessageSubType
lm_message_get_sub_type (LmMessage *message)
{
	g_return_val_if_fail (message != NULL, LM_MESSAGE_TYPE_UNKNOWN);
	
	return PRIV(message)->sub_type;
}

/**
 * lm_message_get_node:
 * @message: an #LmMessage
 * 
 * Retrieves the root node from @message.
 * 
 * Return value: an #LmMessageNode
 **/
LmMessageNode *
lm_message_get_node (LmMessage *message)
{
	g_return_val_if_fail (message != NULL, NULL);
	
	return message->node;
}

/**
 * lm_message_ref:
 * @message: an #LmMessage
 * 
 * Adds a reference to @message.
 * 
 * Return value: the message
 **/
LmMessage *
lm_message_ref (LmMessage *message)
{
	g_return_val_if_fail (message != NULL, NULL);
	
	PRIV(message)->ref_count++;
	
	return message;
}

/**
 * lm_message_unref:
 * @message: an #LmMessage
 * 
 * Removes a reference from @message. When no more references are present the 
 * message is freed.
 **/
void
lm_message_unref (LmMessage *message)
{
	g_return_if_fail (message != NULL);

	PRIV(message)->ref_count--;
	
	if (PRIV(message)->ref_count == 0) {
		lm_message_node_unref (message->node);
		g_free (message->priv);
		g_free (message);
	}
}
