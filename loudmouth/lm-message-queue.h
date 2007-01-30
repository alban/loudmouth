/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#ifndef __LM_MESSAGE_QUEUE_H__
#define __LM_MESSAGE_QUEUE_H__

#include <glib.h>
#include <loudmouth/lm-message.h>

typedef struct _LmMessageQueue LmMessageQueue;

typedef void (* LmMessageQueueCallback) (LmMessageQueue *queue,
					 gpointer        user_data);

LmMessageQueue *  lm_message_queue_new         (LmMessageQueueCallback func,
						gpointer               data);
void              lm_message_queue_attach      (LmMessageQueue        *queue,
						GMainContext *context);

void              lm_message_queue_detach      (LmMessageQueue *queue);
void              lm_message_queue_push_tail   (LmMessageQueue *queue,
						LmMessage      *m);
LmMessage *       lm_message_queue_peek_nth    (LmMessageQueue *queue,
						guint           n);
LmMessage *       lm_message_queue_pop_nth     (LmMessageQueue *queue,
						guint           n);
guint             lm_message_queue_get_length  (LmMessageQueue *queue);
gboolean          lm_message_queue_is_empty    (LmMessageQueue *queue);

LmMessageQueue *  lm_message_queue_ref         (LmMessageQueue *queue);
void              lm_message_queue_unref       (LmMessageQueue *queue);

#endif /* __LM_MESSAGE_QUEUE_H__ */
