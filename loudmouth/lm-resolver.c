/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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

#include "lm-marshal.h"
#include "lm-resolver.h"

static void    resolver_base_init (LmResolverIface *iface);

enum {
        READABLE,
        WRITABLE,
        DISCONNECTED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
lm_resolver_get_type (void)
{
	static GType iface_type = 0;

	if (!iface_type) {
		static const GTypeInfo iface_info = {
			sizeof (LmResolverIface),
			(GBaseInitFunc)     resolver_base_init,
			(GBaseFinalizeFunc) NULL,
		};

		iface_type = g_type_register_static (G_TYPE_INTERFACE,
						     "LmResolverIface",
						     &iface_info,
						     0);

		g_type_interface_add_prerequisite (iface_type, G_TYPE_OBJECT);
	}

	return iface_type;
}

static void
resolver_base_init (LmResolverIface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
                signals[READABLE] =
                        g_signal_new ("readable",
                                      LM_TYPE_RESOLVER,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      lm_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);
                signals[WRITABLE] = 
                        g_signal_new ("writable",
                                      LM_TYPE_RESOLVER,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      lm_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);
                signals[DISCONNECTED] =
                        g_signal_new ("disconnected",
                                      LM_TYPE_RESOLVER,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      lm_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);
		initialized = TRUE;
	}
}

LmResolver *
lm_resolver_new (LmResolverCallback callback, gpointer user_data)
{
        return NULL;
}

void 
lm_resolver_lookup_host (LmResolver  *resolver, const gchar *host)
{
}

void 
lm_resolver_lookup_srv (LmResolver  *resolver,
                        const gchar *domain,
                        const gchar *srv)
{
}

void 
lm_resolver_cancel (LmResolver *resolver)
{
}

