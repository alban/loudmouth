/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2008 Imendio AB
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

#include <stdlib.h>
#include <glib.h>

#include "loudmouth/lm-parser.h"

static GSList *
get_files (const gchar *prefix) 
{
	GSList      *list = NULL;
	GError      *error = NULL;
	GDir        *dir;
	const gchar *name;
	
	dir = g_dir_open (PARSER_TEST_DIR, 0, &error);

	if (!dir) {
		g_error ("Failed to open test file directory '%s' due to: %s",
			 PARSER_TEST_DIR, error->message);
		g_clear_error (&error);

		return NULL;
	}
	

	while ((name = g_dir_read_name (dir))) {
		/* Find *.xml */

		if (g_str_has_prefix (name, prefix) &&
		    g_str_has_suffix (name, ".xml")) {
			gchar *file_path;

			file_path = g_strconcat (PARSER_TEST_DIR, "/", name, 
						 NULL);

			list = g_slist_prepend (list, file_path);
		}
	}

	g_dir_close (dir);
	g_clear_error (&error);

	return list;
}

static void
test_parser_with_file (const gchar *file_path, gboolean is_valid)
{
	LmParser *parser;
	gchar    *file_contents;
	GError   *error = NULL;
	gsize     length;

	parser = lm_parser_new (NULL, NULL, NULL);
	if (!g_file_get_contents (file_path,
				  &file_contents, &length, 
				  &error)) {
		g_error ("Couldn't read file '%s': %s",
			 file_path, error->message);
		g_clear_error (&error);
		return;
	}
	    
	lm_parser_parse (parser, file_contents);
	lm_parser_free (parser);
	g_free (file_contents);
}

static void
test_valid_suite ()
{
	GSList *list, *l;

	list = get_files ("valid");
	for (l = list; l; l = l->next) {
		g_print ("VALID: %s\n", (const gchar *) l->data);
		test_parser_with_file ((const gchar *) l->data, TRUE);
		g_free (l->data);
	}
	g_slist_free (list);
}

static void
test_invalid_suite ()
{
	GSList *list, *l;

	list = get_files ("invalid");
	for (l = list; l; l = l->next) {
		g_print ("INVALID: %s\n", (const gchar *) l->data);
		g_free (l->data);
	}
	g_slist_free (list);
}

int 
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	
	g_test_add_func ("/parser/valid_suite", test_valid_suite);
	g_test_add_func ("/parser/invalid/suite", test_invalid_suite);

	return g_test_run ();
}

