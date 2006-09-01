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

#include "lm-internals.h"
#include "lm-message-handler.h"

struct LmMessageHandler {
	gboolean                valid;
        gint                    ref_count;
        LmHandleMessageFunction function;
        gpointer                user_data;
        GDestroyNotify          notify;
};

LmHandlerResult 
_lm_message_handler_handle_message (LmMessageHandler *handler,
                                    LmConnection     *connection,
                                    LmMessage        *message)
{
        g_return_val_if_fail (handler != NULL, 
                          LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

	if (!handler->valid) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
        
        if (handler->function) {
                return (* handler->function) (handler, connection, 
                                              message, handler->user_data);
        }
        
        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

/**
 * lm_message_handler_new:
 * @function: a callback
 * @user_data: user data passed to function
 * @notify: function called when the message handler is freed
 * 
 * Creates a new message handler. This can be set to handle incoming messages
 * and when a message of the type the handler is registered to handle is
 * received @function will be called and @user_data will be passed to it.
 * @notify is called when the message handler is freed, that way any memory
 * allocated by @user_data can be freed.
 * 
 * Return value: a newly created message handler
 **/
LmMessageHandler *
lm_message_handler_new (LmHandleMessageFunction function,
                        gpointer                user_data,
                        GDestroyNotify          notify)
{
        LmMessageHandler *handler;

	g_return_val_if_fail (function != NULL, NULL);
        
        handler = g_new0 (LmMessageHandler, 1);
        
        if (handler == NULL) {
                return NULL;
        }
        
	handler->valid     = TRUE;	
        handler->ref_count = 1;
        handler->function  = function;
        handler->user_data = user_data;
        handler->notify    = notify;
        
        return handler;
}

/**
 * lm_message_handler_invalidate
 * @handler: an #LmMessageHandler
 *
 * Invalidates the handler. Useful if you need to cancel a reply
 **/
void
lm_message_handler_invalidate (LmMessageHandler *handler)
{
	handler->valid = FALSE;
}

/**
 * lm_message_handler_is_valid
 * @handler: an #LmMessageHandler
 *
 * Fetches whether the handler is valid or not.
 *
 * Return value: #TRUE if @handler is valid, otherwise #FALSE
 **/
gboolean
lm_message_handler_is_valid (LmMessageHandler *handler)
{
	g_return_val_if_fail (handler != NULL, FALSE);

	return handler->valid;
}

/**
 * lm_message_handler_ref:
 * @handler: an #LmMessageHandler
 * 
 * Adds a reference to @handler.
 * 
 * Return value: the message handler
 **/
LmMessageHandler *
lm_message_handler_ref (LmMessageHandler *handler)
{
        g_return_val_if_fail (handler != NULL, NULL);
        
        handler->ref_count++;

        return handler;
}

/**
 * lm_message_handler_unref:
 * @handler: an #LmMessagHandler
 * 
 * Removes a reference from @handler. When no more references are present the 
 * handler is freed.
 **/
void
lm_message_handler_unref (LmMessageHandler *handler)
{
        g_return_if_fail (handler != NULL);
        
        handler->ref_count --;
        
        if (handler->ref_count == 0) {
                if (handler->notify) {
                        (* handler->notify) (handler->user_data);
                }
                g_free (handler);
        }
}

