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
#include "lm-message-node.h"

typedef struct {
        gchar *key;
        gchar *value;
} KeyValuePair;

static void            message_node_free            (LmMessageNode    *node);
static LmMessageNode * message_node_last_child      (LmMessageNode    *node);

static void
message_node_free (LmMessageNode *node)
{
        LmMessageNode *l;
        GSList        *list;
        
        g_return_if_fail (node != NULL);

	for (l = node->children; l;) {
		LmMessageNode *next = l->next;

		lm_message_node_unref (l);
		l = next;
        }

        g_free (node->name);
        g_free (node->value);
        
        for (list = node->attributes; list; list = list->next) {
                KeyValuePair *kvp = (KeyValuePair *) list->data;
                
                g_free (kvp->key);
                g_free (kvp->value);
                g_free (kvp);
        }
        
        g_slist_free (node->attributes);
        g_free (node);
}

static LmMessageNode *
message_node_last_child (LmMessageNode *node)
{
        LmMessageNode *l;
        
        g_return_val_if_fail (node != NULL, NULL);

        if (!node->children) {
                return NULL;
        }
                
        l = node->children;

        while (l->next) {
                l = l->next;
        }

        return l;
}

LmMessageNode *
_lm_message_node_new (const gchar *name)
{
        LmMessageNode *node;

        node = g_new0 (LmMessageNode, 1);
        
        node->name       = g_strdup (name);
        node->value      = NULL;
	node->raw_mode   = FALSE;
        node->attributes = NULL;
        node->next       = NULL;
        node->prev       = NULL;
        node->parent     = NULL;
        node->children   = NULL;

	node->ref_count  = 1;

        return node;
}
void
_lm_message_node_add_child_node (LmMessageNode *node, LmMessageNode *child)
{
        LmMessageNode *prev;
	
        g_return_if_fail (node != NULL);

        prev = message_node_last_child (node);
	lm_message_node_ref (child);

        if (prev) {
                prev->next    = child;
                child->prev   = prev;
        } else {
                node->children = child;
        }
        
        child->parent = node;
}

/**
 * lm_message_node_get_value:
 * @node: an #LmMessageNode
 * 
 * Retrieves the value of @node.
 * 
 * Return value: 
 **/
const gchar *
lm_message_node_get_value (LmMessageNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	
	return node->value;
}

/**
 * lm_message_node_set_value:
 * @node: an #LmMessageNode
 * @value: the new value.
 * 
 * Sets the value of @node. If a previous value is set it will be freed.
 **/
void
lm_message_node_set_value (LmMessageNode *node, const gchar *value)
{
        g_return_if_fail (node != NULL);
       
        g_free (node->value);
	
        if (!value) {
                node->value = NULL;
                return;
        }

        node->value = g_strdup (value);
}

/**
 * lm_message_node_add_child:
 * @node: an #LmMessageNode
 * @name: the name of the new child
 * @value: value of the new child
 * 
 * Add a child node with @name and value set to @value. 
 * 
 * Return value: the newly created child
 **/
LmMessageNode *
lm_message_node_add_child (LmMessageNode *node, 
			   const gchar   *name, 
			   const gchar   *value)
{
	LmMessageNode *child;
	
	child = _lm_message_node_new (name);

	lm_message_node_set_value (child, value);
	_lm_message_node_add_child_node (node, child);
	lm_message_node_unref (child);

	return child;
}

/**
 * lm_message_node_set_attributes:
 * @node: an #LmMessageNode
 * @name: the first attribute, should be followed by a string with the value
 * @Varargs: The rest of the name/value pairs
 * 
 * Sets a list of attributes. The arguments should be names and corresponding 
 * value and needs to be ended with %NULL.
 **/
void
lm_message_node_set_attributes  (LmMessageNode *node,
				 const gchar   *name,
				 ...)
{
	va_list args;
	
        g_return_if_fail (node != NULL);

	for (va_start (args, name); 
	     name; 
	     name = (const gchar *) va_arg (args, gpointer)) {
		const gchar *value;

		value = (const gchar *) va_arg (args, gpointer);

		lm_message_node_set_attribute (node, name, value);
		
	}

	va_end (args);
}

/**
 * lm_message_node_set_attribute:
 * @node: an #LmMessageNode
 * @name: name of attribute
 * @value: value of attribute.
 * 
 * Sets the attribute @name to @value.
 **/
void
lm_message_node_set_attribute (LmMessageNode *node,
			       const gchar   *name,
			       const gchar   *value)
{
	gboolean  found = FALSE; 
	GSList   *l;

	for (l = node->attributes; l; l = l->next) {
		KeyValuePair *kvp = (KeyValuePair *) l->data;
                
		if (strcmp (kvp->key, name) == 0) {
			g_free (kvp->value);
			kvp->value = g_strdup (value);
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		KeyValuePair *kvp;
	
		kvp = g_new0 (KeyValuePair, 1);                
		kvp->key = g_strdup (name);
		kvp->value = g_strdup (value);
		
		node->attributes = g_slist_prepend (node->attributes, kvp);
	}
}

/**
 * lm_message_node_get_attribute:
 * @node: an #LmMessageNode
 * @name: the attribute name
 * 
 * Fetches the attribute @name from @node.
 * 
 * Return value: the attribute value or %NULL if not set
 **/
const gchar *
lm_message_node_get_attribute (LmMessageNode *node, const gchar *name)
{
        GSList      *l;
        const gchar *ret_val = NULL;

        g_return_val_if_fail (node != NULL, NULL);

        for (l = node->attributes; l; l = l->next) {
                KeyValuePair *kvp = (KeyValuePair *) l->data;
                
                if (strcmp (kvp->key, name) == 0) {
                        ret_val = kvp->value;
                }
        }
        
        return ret_val;
}

/**
 * lm_message_node_get_child:
 * @node: an #LmMessageNode
 * @child_name: the childs name
 * 
 * Fetches the child @child_name from @node. If child is not found as an 
 * immediate child of @node %NULL is returned.
 * 
 * Return value: the child node or %NULL if not found
 **/
LmMessageNode *
lm_message_node_get_child (LmMessageNode *node, const gchar *child_name)
{
	LmMessageNode *l;
	
	for (l = node->children; l; l = l->next) {
		if (strcmp (l->name, child_name) == 0) {
			return l;
		}
	}

	return NULL;
}

/**
 * lm_message_node_find_child:
 * @node: A #LmMessageNode
 * @child_name: The name of the child to find
 * 
 * Locates a child among all children of @node. The entire tree will be search 
 * until a child with name @child_name is located. 
 * 
 * Return value: the located child or %NULL if not found
 **/
LmMessageNode * 
lm_message_node_find_child (LmMessageNode *node,
			    const gchar   *child_name)
{
        LmMessageNode *l;
        LmMessageNode *ret_val = NULL;

        for (l = node->children; l; l = l->next) {
                if (strcmp (l->name, child_name) == 0) {
                        return l;
                }
                if (l->children) {
                        ret_val = lm_message_node_find_child (l, child_name);
                        if (ret_val) {
                                return ret_val;
                        }
                }
        }

        return NULL;
}

/**
 * lm_message_node_get_raw_mode:
 * @node: an #LmMessageNode
 * 
 * Checks if the nodes value should be sent as raw mode. 
 *
 * Return value: %TRUE if nodes value should be sent as is and %FALSE if the value will be escaped before sending.
 **/
gboolean
lm_message_node_get_raw_mode (LmMessageNode *node)
{
	g_return_val_if_fail (node != NULL, FALSE);

	return node->raw_mode;
}

/** 
 * lm_message_node_set_raw_mode:
 * @node: an #LmMessageNode
 * @raw_mode: boolean specifying if node value should be escaped or not.
 *
 * Set @raw_mode to %TRUE if you don't want to escape the value. You need to make sure the value is valid XML yourself.
 **/
void
lm_message_node_set_raw_mode (LmMessageNode *node, gboolean raw_mode)
{
	g_return_if_fail (node != NULL);

	node->raw_mode = raw_mode;	
}

/**
 * lm_message_node_ref:
 * @node: an #LmMessageNode
 * 
 * Adds a reference to @node.
 * 
 * Return value: the node
 **/
LmMessageNode *
lm_message_node_ref (LmMessageNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	
	node->ref_count++;
       
	return node;
}

/**
 * lm_message_node_unref:
 * @node: an #LmMessageNode
 * 
 * Removes a reference from @node. When no more references are present the
 * node is freed. When freed lm_message_node_unref() will be called on all
 * children. If caller needs to keep references to the children a call to 
 * lm_message_node_ref() needs to be done before the call to 
 *lm_message_unref().
 **/
void
lm_message_node_unref (LmMessageNode *node)
{
	g_return_if_fail (node != NULL);
	
	node->ref_count--;
	
	if (node->ref_count == 0) {
		message_node_free (node);
	}
}

/**
 * lm_message_node_to_string:
 * @node: an #LmMessageNode
 * 
 * Returns an XML string representing the node. This is what is sent over the
 * wire. This is used internally Loudmouth and is external for debugging 
 * purposes.
 * 
 * Return value: an XML string representation of @node
 **/
gchar *
lm_message_node_to_string (LmMessageNode *node)
{
	gchar         *ret_val;
	gchar         *str;
	GSList        *l;
	LmMessageNode *child;
	
	if (node->name == NULL) {
		return g_strdup ("");
	}
	
	str = g_strdup_printf ("<%s", node->name);
	
	for (l = node->attributes; l; l = l->next) {
		KeyValuePair *kvp = (KeyValuePair *) l->data;
		
		ret_val = g_strdup_printf ("%s %s=\"%s\"", 
					   str, kvp->key, kvp->value);
		g_free (str);
		str = ret_val;
	}
	
	ret_val = g_strconcat (str, ">", NULL);
	g_free (str);
	
	if (node->value) {
		gchar *tmp;

		str = ret_val;

		if (node->raw_mode == FALSE) {
			tmp = g_markup_escape_text (node->value, -1);
			ret_val = g_strconcat (str, tmp, NULL);
			g_free (tmp);
		} else {
			ret_val = g_strconcat (str, node->value, NULL);
		}
		g_free (str);
	} 

	for (child = node->children; child; child = child->next) {
		gchar *child_str = lm_message_node_to_string (child);
		str = ret_val;
		ret_val = g_strconcat (str, "  ", child_str, NULL);
		g_free (str);
		g_free (child_str);
	}

	str = ret_val;
	ret_val = g_strdup_printf ("%s</%s>\n", str, node->name);
	g_free (str);
	
	return ret_val;
}
