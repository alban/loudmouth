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

#include <string.h>

#include "lm-misc.h"

static void
misc_setup_source (GMainContext *context,
		   GSource      *source,
		   GSourceFunc   function,
		   gpointer      data)
{
	g_source_set_callback (source, (GSourceFunc)function, data, NULL);
	g_source_attach (source, context);
	g_source_unref (source);
}

GSource *
lm_misc_add_io_watch (GMainContext *context,
		      GIOChannel   *channel,
		      GIOCondition  condition,
		      GIOFunc       function,
		      gpointer      data)
{
	GSource *source;
                                                                                
	g_return_val_if_fail (channel != NULL, 0);
                                                                                
	source = g_io_create_watch (channel, condition);
	misc_setup_source (context, source, (GSourceFunc) function, data);

	return source;
}

GSource *
lm_misc_add_idle (GMainContext *context,
		  GSourceFunc   function,
		  gpointer      data)
{
	GSource *source;
                                                                                
	g_return_val_if_fail (function != NULL, 0);
                                                                                
	source = g_idle_source_new ();
	misc_setup_source (context, source, function, data);
  
	return source;
}

GSource *
lm_misc_add_timeout (GMainContext *context,
		     guint         interval,
		     GSourceFunc   function,
		     gpointer      data)
{
	GSource *source;
                                                                                
	g_return_val_if_fail (function != NULL, 0);
                                                                                
	source = g_timeout_source_new (interval);
	misc_setup_source (context, source, function, data);
  
	return source;
}

const char *
lm_misc_io_condition_to_str  (GIOCondition condition)
{
	static char buf[256];

	buf[0] = '\0';

	if(condition & G_IO_ERR)
		strcat(buf, "G_IO_ERR ");
	if(condition & G_IO_HUP)
		strcat(buf, "G_IO_HUP ");
	if(condition & G_IO_NVAL)
		strcat(buf, "G_IO_NVAL ");
	if(condition & G_IO_IN)
		strcat(buf, "G_IO_IN ");
	if(condition & G_IO_OUT)
		strcat(buf, "G_IO_OUT ");

	return buf;
}

