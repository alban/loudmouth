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

#include "lm-debug.h"

#ifndef LM_NO_DEBUG

static LmLogLevelFlags debug_flags = 0;
static gboolean initialized = FALSE;

static const GDebugKey debug_keys[] = {
	{"VERBOSE",      LM_LOG_LEVEL_VERBOSE},
	{"NET",          LM_LOG_LEVEL_NET},
	{"PARSER",       LM_LOG_LEVEL_PARSER},
	{"SSL",          LM_LOG_LEVEL_SSL},
	{"SASL",         LM_LOG_LEVEL_SASL},
	{"ALL",          LM_LOG_LEVEL_ALL}
};

#define NUM_DEBUG_KEYS (sizeof (debug_keys) / sizeof (GDebugKey))

static void
debug_log_handler (const gchar    *log_domain,
		   GLogLevelFlags  log_level,
		   const gchar    *message,
		   gpointer        user_data)
{
	if (debug_flags & log_level) {
		if (log_level & LM_LOG_LEVEL_VERBOSE) {
			g_print ("*** ");
		}
		else if (log_level & LM_LOG_LEVEL_PARSER) {
			g_print ("LM-PARSER: ");
		} 
		else if (log_level & LM_LOG_LEVEL_SASL) {
			g_print ("LM-SASL: ");
		}
		else if (log_level & LM_LOG_LEVEL_SSL) {
			g_print ("LM-SSL: ");
		}
	
		g_print ("%s", message);
	}
}

/**
 * lm_debug_init
 *
 * Initialized the debug system.
 **/
void 
lm_debug_init (void)
{
	const gchar *env_lm_debug;

	if (initialized) {
		return;
	}
	
	env_lm_debug = g_getenv ("LM_DEBUG");
	if (env_lm_debug) {
		debug_flags = g_parse_debug_string (env_lm_debug, debug_keys,
						    NUM_DEBUG_KEYS);
	}

	g_log_set_handler (LM_LOG_DOMAIN, LM_LOG_LEVEL_ALL, 
			   debug_log_handler, NULL);

	initialized = TRUE;
}

#else  /* LM_NO_DEBUG */

void 
lm_debug_init (void)
{
}

#endif /* LM_NO_DEBUG */

