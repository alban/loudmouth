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

#include <glib.h>

#include "lm-debug.h"
#include "lm-internals.h"
#include "lm-message-node.h"
#include "lm-parser.h"

#define SHORT_END_TAG "/>"
#define XML_MAX_DEPTH 5

#define LM_PARSER(o) ((LmParser *) o)

struct LmParser {
	LmParserMessageFunction  function;
	gpointer                 user_data;
	GDestroyNotify           notify;
	
	LmMessageNode           *cur_root;
	LmMessageNode           *cur_node;
		
	GMarkupParser           *m_parser;
	GMarkupParseContext     *context;
};


/* Used while parsing */
static void    parser_start_node_cb (GMarkupParseContext  *context,
				     const gchar          *node_name,
				     const gchar         **attribute_names,
				     const gchar         **attribute_values,
				     gpointer              user_data,
				     GError              **error);
static void    parser_end_node_cb   (GMarkupParseContext  *context,
				     const gchar          *node_name,
				     gpointer              user_data,
				     GError              **error);
static void    parser_text_cb       (GMarkupParseContext  *context,
				     const gchar          *text,
				     gsize                 text_len,  
				     gpointer              user_data,
				     GError              **error);
static void    parser_error_cb      (GMarkupParseContext  *context,
				     GError               *error,
				     gpointer              user_data);

static void
parser_start_node_cb (GMarkupParseContext  *context,
		      const gchar          *node_name,
		      const gchar         **attribute_names,
		      const gchar         **attribute_values,
		      gpointer              user_data,
		      GError              **error)
{	
	LmParser     *parser;
	gint          i;
	
	parser = LM_PARSER (user_data);;
	

/* 	parser->cur_depth++; */

	if (!parser->cur_root) {
		/* New toplevel element */
		parser->cur_root = _lm_message_node_new (node_name);
		parser->cur_node = parser->cur_root;
	} else {
		LmMessageNode *parent_node;
		
		parent_node = parser->cur_node;
		
		parser->cur_node = _lm_message_node_new (node_name);
		_lm_message_node_add_child_node (parent_node,
						 parser->cur_node);
	}

	for (i = 0; attribute_names[i]; ++i) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_PARSER, 
		       "ATTRIBUTE: %s = %s\n", 
		       attribute_names[i],
		       attribute_values[i]);
		
		lm_message_node_set_attributes (parser->cur_node,
						attribute_names[i],
						attribute_values[i], 
						NULL);
	}
	
	if (strcmp ("stream:stream", node_name) == 0) {
		parser_end_node_cb (context,
				    "stream:stream",
				    user_data, 
				    error);
	}
}

static void
parser_end_node_cb (GMarkupParseContext  *context,
		    const gchar          *node_name,
		    gpointer              user_data,
		    GError              **error)
{
	LmParser     *parser;
	
	parser = LM_PARSER (user_data);
	
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_PARSER,
	       "Trying to close node: %s\n", node_name);

        if (!parser->cur_node) {
                /* FIXME: LM-1 should look at this */
                return;
        }
        
	if (strcmp (parser->cur_node->name, node_name) != 0) {
		if (strcmp (node_name, "stream:stream")) {
			g_print ("Got an stream:stream end\n");
		}
		g_warning ("Trying to close node that isn't open: %s",
			   node_name);
		return;
	}

	if (parser->cur_node == parser->cur_root) {
		LmMessage *m;
		
		m = _lm_message_new_from_node (parser->cur_root);

		if (!m) {
			g_warning ("Couldn't create message: %s\n",
				   parser->cur_root->name);
			return;
		}

		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_PARSER,
		       "Have a new message\n");
		if (parser->function) {
			(* parser->function) (parser, m, parser->user_data);
		}

		lm_message_unref (m);
		lm_message_node_unref (parser->cur_root);
		
			
		parser->cur_node = parser->cur_root = NULL;
	} else {
		LmMessageNode *tmp_node;
		tmp_node = parser->cur_node;
		parser->cur_node = parser->cur_node->parent;

		lm_message_node_unref (tmp_node);
	}
}

static void
parser_text_cb (GMarkupParseContext   *context,
		const gchar           *text,
		gsize                  text_len,  
		gpointer               user_data,
		GError               **error)
{
	LmParser *parser;
	
	g_return_if_fail (user_data != NULL);
	
	parser = LM_PARSER (user_data);
	
	if (parser->cur_node && strcmp (text, "") != 0) {
		lm_message_node_set_value (parser->cur_node, text);
	} 
}

static void
parser_error_cb (GMarkupParseContext *context,
		 GError              *error,
		 gpointer             user_data)
{
	g_return_if_fail (user_data != NULL);
	g_return_if_fail (error != NULL);
	
	g_warning ("Parsing failed: %s\n", error->message);
}

LmParser *
lm_parser_new (LmParserMessageFunction function, 
	       gpointer                user_data, 
	       GDestroyNotify          notify)
{
	LmParser *parser;
	
	parser = g_new0 (LmParser, 1);
	if (!parser) {
		return NULL;
	}
	
	parser->m_parser = g_new0 (GMarkupParser, 1);
	if (!parser->m_parser) {
		g_free (parser);
		return NULL;
	}

	parser->function  = function;
	parser->user_data = user_data;
	parser->notify    = notify;
	
	parser->m_parser->start_element = parser_start_node_cb;
	parser->m_parser->end_element   = parser_end_node_cb;
	parser->m_parser->text          = parser_text_cb;
	parser->m_parser->error         = parser_error_cb;

	parser->context = g_markup_parse_context_new (parser->m_parser, 0,
						      parser, NULL);

	parser->cur_root = NULL;
	parser->cur_node = NULL;

	return parser;
}

void
lm_parser_parse (LmParser *parser, const gchar *string)
{
	g_return_if_fail (parser != NULL);
	
        if (!parser->context) {
                parser->context = g_markup_parse_context_new (parser->m_parser, 0,
                                                              parser, NULL);
        }
        
        if (g_markup_parse_context_parse (parser->context, string, 
                                          (gssize)strlen (string), NULL)) {
        } else {
		g_markup_parse_context_free (parser->context);
		parser->context = NULL;
        }
}

void
lm_parser_free (LmParser *parser)
{
	if (parser->notify) {
		(* parser->notify) (parser->user_data);
	}

	if (parser->context) {
		g_markup_parse_context_free (parser->context);
	}
	g_free (parser->m_parser);
	g_free (parser);
}

