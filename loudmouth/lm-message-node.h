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

#ifndef __LM_MESSAGE_NODE_H__
#define __LM_MESSAGE_NODE_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _LmMessageNode LmMessageNode;

struct _LmMessageNode {
	gchar      *name;
	gchar      *value;
	gboolean    raw_mode;

        LmMessageNode     *next;
        LmMessageNode     *prev;
	LmMessageNode     *parent;
        LmMessageNode     *children;

	/* < private > */
	GSList     *attributes;
	gint        ref_count;
};

const gchar *  lm_message_node_get_value      (LmMessageNode *node);
void           lm_message_node_set_value      (LmMessageNode *node,
					       const gchar   *value);
LmMessageNode *lm_message_node_add_child      (LmMessageNode *node,
					       const gchar   *name,
					       const gchar   *value);
void           lm_message_node_set_attributes (LmMessageNode *node,
					       const gchar   *name,
					       ...);
void           lm_message_node_set_attribute  (LmMessageNode *node,
					       const gchar   *name,
					       const gchar   *value);
const gchar *  lm_message_node_get_attribute  (LmMessageNode *node,
					       const gchar   *name);
LmMessageNode *lm_message_node_get_child      (LmMessageNode *node,
					       const gchar   *child_name);
LmMessageNode *lm_message_node_find_child     (LmMessageNode *node,
					       const gchar   *child_name);
gboolean       lm_message_node_get_raw_mode   (LmMessageNode *node);
void           lm_message_node_set_raw_mode   (LmMessageNode *node,
					       gboolean       raw_mode);
LmMessageNode *lm_message_node_ref            (LmMessageNode *node);
void           lm_message_node_unref          (LmMessageNode *node);
gchar *        lm_message_node_to_string      (LmMessageNode *node);

G_END_DECLS

#endif /* __LM_MESSAGE_NODE_H__ */
