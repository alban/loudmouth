/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * LmQueue: Double ended queue implementation, piggy backed on GList.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This file is copied from gqueue.c in Glib */

#include "config.h"

#include "glib.h"
#include "lm-queue.h"

G_LOCK_DEFINE_STATIC (queue_memchunk);
static GMemChunk   *queue_memchunk = NULL;
static GTrashStack *free_queue_nodes = NULL;

LmQueue*
lm_queue_new (void)
{
	LmQueue *queue;

	G_LOCK (queue_memchunk);
	queue = g_trash_stack_pop (&free_queue_nodes);

	if (!queue)
	{
		if (!queue_memchunk)
			queue_memchunk = g_mem_chunk_new ("GLib LmQueue chunk",
							  sizeof (GNode),
							  sizeof (GNode) * 128,
							  G_ALLOC_ONLY);
		queue = g_chunk_new (LmQueue, queue_memchunk);
	}
	G_UNLOCK (queue_memchunk);

	queue->head = NULL;
	queue->tail = NULL;
	queue->length = 0;

	return queue;
}

void
lm_queue_free (LmQueue *queue)
{
	g_return_if_fail (queue != NULL);

	g_list_free (queue->head);

#ifdef ENABLE_GC_FRIENDLY
	queue->head = NULL;
	queue->tail = NULL;
#endif /* ENABLE_GC_FRIENDLY */

	G_LOCK (queue_memchunk);
	g_trash_stack_push (&free_queue_nodes, queue);
	G_UNLOCK (queue_memchunk);
}

void
lm_queue_push_head (LmQueue   *queue,
		    LmMessage *m)
{
	g_return_if_fail (queue != NULL);

	queue->head = g_list_prepend (queue->head, m);
	if (!queue->tail)
		queue->tail = queue->head;
	queue->length++;
}

void
lm_queue_push_head_link (LmQueue *queue,
			 GList  *link)
{
	g_return_if_fail (queue != NULL);
	g_return_if_fail (link != NULL);
	g_return_if_fail (link->prev == NULL);
	g_return_if_fail (link->next == NULL);

	link->next = queue->head;
	if (queue->head)
		queue->head->prev = link;
	else
		queue->tail = link;
	queue->head = link;
	queue->length++;
}

void
lm_queue_push_tail (LmQueue  *queue,
		    LmMessage *m)
{
	g_return_if_fail (queue != NULL);

	queue->tail = g_list_append (queue->tail, m);
	if (queue->tail->next)
		queue->tail = queue->tail->next;
	else
		queue->head = queue->tail;
	queue->length++;
}

void
lm_queue_push_tail_link (LmQueue *queue,
			 GList  *link)
{
	g_return_if_fail (queue != NULL);
	g_return_if_fail (link != NULL);
	g_return_if_fail (link->prev == NULL);
	g_return_if_fail (link->next == NULL);

	link->prev = queue->tail;
	if (queue->tail)
		queue->tail->next = link;
	else
		queue->head = link;
	queue->tail = link;
	queue->length++;
}

LmMessage *
lm_queue_pop_head (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	if (queue->head)
	{
		GList *node = queue->head;
		gpointer data = node->data;

		queue->head = node->next;
		if (queue->head)
			queue->head->prev = NULL;
		else
			queue->tail = NULL;
		g_list_free_1 (node);
		queue->length--;

		return (LmMessage *) data;
	}

	return NULL;
}

GList*
lm_queue_pop_head_link (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	if (queue->head)
	{
		GList *node = queue->head;

		queue->head = node->next;
		if (queue->head)
		{
			queue->head->prev = NULL;
			node->next = NULL;
		}
		else
			queue->tail = NULL;
		queue->length--;

		return node;
	}

	return NULL;
}

LmMessage *
lm_queue_pop_tail (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	if (queue->tail)
	{
		GList *node = queue->tail;
		gpointer data = node->data;

		queue->tail = node->prev;
		if (queue->tail)
			queue->tail->next = NULL;
		else
			queue->head = NULL;
		queue->length--;
		g_list_free_1 (node);

		return (LmMessage *) data;
	}

	return NULL;
}

GList*
lm_queue_pop_tail_link (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	if (queue->tail)
	{
		GList *node = queue->tail;

		queue->tail = node->prev;
		if (queue->tail)
		{
			queue->tail->next = NULL;
			node->prev = NULL;
		}
		else
			queue->head = NULL;
		queue->length--;

		return node;
	}

	return NULL;
}

gboolean
lm_queue_is_empty (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, TRUE);

	return queue->head == NULL;
}

LmMessage *
lm_queue_peek_head (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	return (LmMessage *) queue->head ? queue->head->data : NULL;
}

LmMessage *
lm_queue_peek_tail (LmQueue *queue)
{
	g_return_val_if_fail (queue != NULL, NULL);

	return (LmMessage *) queue->tail ? queue->tail->data : NULL;
}

LmMessage * 
lm_queue_peek_nth (LmQueue *queue, int n)
{
	gpointer data;
	
	g_return_val_if_fail (queue != NULL, NULL);
	g_return_val_if_fail (n >= 0, NULL);

	data = g_list_nth_data (queue->head, n);

	return (LmMessage *) data;
}

LmMessage *
lm_queue_remove_nth (LmQueue *queue, int n)
{
	GList    *link;
	gpointer  data;

	g_return_val_if_fail (queue != NULL, NULL);
	g_return_val_if_fail (n >= 0, NULL);
	g_return_val_if_fail (n < queue->length, NULL);

	if (n == 0) {
		return lm_queue_pop_head (queue);
	}
	
	if (n == queue->length - 1) {
		return lm_queue_pop_tail (queue);
	}

	link = g_list_nth (queue->head, n);
	data = link->data;

	queue->head = g_list_delete_link (queue->head, link);
	queue->length--;
	
	return (LmMessage *) data;
}

