/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* This file is copied from gqueue.h in Glib */

#ifndef __lm_queue_H__
#define __lm_queue_H__

#include <glib/glist.h>

#include "lm-message.h"

G_BEGIN_DECLS

typedef struct _LmQueue		LmQueue;

struct _LmQueue
{
  GList *head;
  GList *tail;
  guint  length;
};

/* Queues
 */
LmQueue*    lm_queue_new            (void);
void        lm_queue_free           (LmQueue  *queue);
void        lm_queue_push_head      (LmQueue  *queue,
				     LmMessage *message);
void        lm_queue_push_tail      (LmQueue  *queue,
				     LmMessage *message);
LmMessage * lm_queue_pop_head       (LmQueue  *queue);
LmMessage * lm_queue_pop_tail       (LmQueue  *queue);
gboolean    lm_queue_is_empty       (LmQueue  *queue);
LmMessage * lm_queue_peek_head      (LmQueue  *queue);
LmMessage * lm_queue_peek_tail      (LmQueue  *queue);
void        lm_queue_push_head_link (LmQueue  *queue,
				     GList   *link_);
void        lm_queue_push_tail_link (LmQueue  *queue,
				     GList   *link_);

LmMessage * lm_queue_peek_nth       (LmQueue  *queue,
				     int       n);
LmMessage * lm_queue_remove_nth     (LmQueue  *queue,
				     int       n);

GList*      lm_queue_pop_head_link  (LmQueue  *queue);
GList*      lm_queue_pop_tail_link  (LmQueue  *queue); 

G_END_DECLS

#endif /* __lm_queue_H__ */
