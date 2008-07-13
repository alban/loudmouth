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
#include "lm-blocking-resolver.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_BLOCKING_RESOLVER, LmBlockingResolverPriv))

typedef struct LmBlockingResolverPriv LmBlockingResolverPriv;
struct LmBlockingResolverPriv {
	gint my_prop;
};

static void     blocking_resolver_finalize    (GObject       *object);
static void     blocking_resolver_lookup      (LmResolver    *resolver);
static void     blocking_resolver_cancel      (LmResolver    *resolver);

G_DEFINE_TYPE (LmBlockingResolver, lm_blocking_resolver, LM_TYPE_RESOLVER)

static void
lm_blocking_resolver_class_init (LmBlockingResolverClass *class)
{
        GObjectClass    *object_class   = G_OBJECT_CLASS (class);
        LmResolverClass *resolver_class = LM_RESOLVER_CLASS (class);

	object_class->finalize = blocking_resolver_finalize;

        resolver_class->lookup = blocking_resolver_lookup;
        resolver_class->cancel = blocking_resolver_cancel;
	
	g_type_class_add_private (object_class, sizeof (LmBlockingResolverPriv));
}

static void
lm_blocking_resolver_init (LmBlockingResolver *blocking_resolver)
{
	LmBlockingResolverPriv *priv;

	priv = GET_PRIV (blocking_resolver);

}

static void
blocking_resolver_finalize (GObject *object)
{
	LmBlockingResolverPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (lm_blocking_resolver_parent_class)->finalize) (object);
}

static void
blocking_resolver_lookup (LmResolver *resolver)
{
}

static void
blocking_resolver_cancel (LmResolver *resolver)
{
}

