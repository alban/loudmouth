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

#include <config.h>

#include "lm-message-queue.h"

struct _LmMessageQueue {
	GQueue                  *messages;

	GMainContext            *context;
	GSource                 *source;

	LmMessageQueueCallback  callback;
	gpointer                user_data;

	gint                    ref_count;
};

typedef struct {
	GSource         source;
	LmMessageQueue *queue;
} MessageQueueSource;

static void        message_queue_free            (LmMessageQueue *queue);
static gboolean    message_queue_prepare_func    (GSource         *source,
						  gint            *timeout);
static gboolean    message_queue_check_func      (GSource         *source);
static gboolean    message_queue_dispatch_func   (GSource         *source,
						  GSourceFunc      callback,
						  gpointer         user_data);

static GSourceFuncs source_funcs = {
	message_queue_prepare_func,
	message_queue_check_func,
	message_queue_dispatch_func,
	NULL
};

static void
foreach_free_message (LmMessage *m, gpointer user_data)
{
	lm_message_unref (m);
}

static void
message_queue_free (LmMessageQueue *queue)
{
	lm_message_queue_detach (queue);
	
	g_queue_foreach (queue->messages, (GFunc) foreach_free_message, NULL);
	g_queue_free (queue->messages);

	g_free (queue);
}

static gboolean
message_queue_prepare_func (GSource *source, gint *timeout)
{
	LmMessageQueue *queue;

	queue = ((MessageQueueSource *)source)->queue;

	return !g_queue_is_empty (queue->messages);
}

static gboolean
message_queue_check_func (GSource *source)
{
	return FALSE;
}

static gboolean
message_queue_dispatch_func (GSource     *source,
			     GSourceFunc  callback,
			     gpointer     user_data)
{
	LmMessageQueue *queue;

	queue = ((MessageQueueSource *)source)->queue;

	if (queue->callback) {
		(queue->callback) (queue, queue->user_data);
	}

	return TRUE;
}

LmMessageQueue *
lm_message_queue_new (LmMessageQueueCallback  callback,
		      gpointer                user_data)
{
	LmMessageQueue *queue;

	queue = g_new0 (LmMessageQueue, 1);

	queue->messages = g_queue_new ();
	queue->context = NULL;
	queue->source = NULL;
	queue->ref_count = 1;

	queue->callback = callback;
	queue->user_data = user_data;

	return queue;
}

void
lm_message_queue_attach (LmMessageQueue *queue, GMainContext *context)
{
	GSource *source;

	if (queue->source) {
		if (queue->context == context) {
			/* Already attached */
			return;
		}
		lm_message_queue_detach (queue);
	}

	if (context)  {
		queue->context = g_main_context_ref (context);
	}
	
	source = g_source_new (&source_funcs, sizeof (MessageQueueSource));
	((MessageQueueSource *)source)->queue = queue;
	queue->source = source;

	g_source_attach (source, queue->context);
}

void
lm_message_queue_detach (LmMessageQueue *queue)
{
	if (queue->source) {
		g_source_destroy (queue->source);
		g_source_unref (queue->source);
	}

	if (queue->context) {
		g_main_context_unref (queue->context);
	}

	queue->source = NULL;
	queue->context = NULL;
}

void
lm_message_queue_push_tail (LmMessageQueue *queue, LmMessage *m)
{
	g_return_if_fail (queue != NULL);
	g_return_if_fail (m != NULL);

	g_queue_push_tail (queue->messages, m);
}

LmMessage *
lm_message_queue_peek_nth (LmMessageQueue *queue, guint n)
{
	g_return_val_if_fail (queue != NULL, NULL);

	return (LmMessage *) g_queue_peek_nth (queue->messages, n);
}

LmMessage *
lm_message_queue_pop_nth (LmMessageQueue *queue, guint n)
{
	g_return_val_if_fail (queue != NULL, NULL);

	return (LmMessage *) g_queue_pop_nth (queue->messages, n);
}

guint
lm_message_queue_get_length (LmMessageQueue *queue)
{
	g_return_val_if_fail (queue != NULL, 0);

	return g_queue_get_length (queue->messages);
}

gboolean 
lm_message_queue_is_empty (LmMessageQueue *queue)
{
	g_return_val_if_fail (queue != NULL, TRUE);

	return g_queue_is_empty (queue->messages);
}

LmMessageQueue *
lm_message_queue_ref (LmMessageQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	queue->ref_count++;

	return queue;
}

void
lm_message_queue_unref (LmMessageQueue *queue)
{
	g_return_if_fail (queue != NULL);

	queue->ref_count--;

	if (queue->ref_count <= 0) {
		message_queue_free (queue);
	}
}

