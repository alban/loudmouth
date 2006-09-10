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

#include <stdlib.h>
#include <check.h> 
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

static Suite *
create_lm_parser_valid_suite ()
{
	Suite  *suite;
	GSList *list, *l;

	suite = suite_create ("LmParser");

	list = get_files ("valid");
	for (l = list; l; l = l->next) {
		g_print ("VALID: %s\n", (const gchar *) l->data);
		test_parser_with_file ((const gchar *) l->data, TRUE);
		g_free (l->data);
	}
	g_slist_free (list);

	return suite;
}

static Suite *
create_lm_parser_invalid_suite ()
{
	Suite  *suite;
	GSList *list, *l;

	suite = suite_create ("LmParser");

	list = get_files ("invalid");
	for (l = list; l; l = l->next) {
		g_print ("INVALID: %s\n", (const gchar *) l->data);
		g_free (l->data);
	}
	g_slist_free (list);

	return suite;
}

int 
main (int argc, char **argv)
{
	SRunner *srunner;
	int      nf;

	srunner = srunner_create (create_lm_parser_valid_suite ());

	srunner_add_suite (srunner, create_lm_parser_invalid_suite ());

	srunner_run_all (srunner, CK_NORMAL);
	nf = srunner_ntests_failed (srunner);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

