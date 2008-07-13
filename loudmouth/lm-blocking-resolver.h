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

#ifndef __LM_BLOCKING_RESOLVER_H__
#define __LM_BLOCKING_RESOLVER_H__

#include <glib-object.h>

#include "lm-resolver.h" 

G_BEGIN_DECLS

#define LM_TYPE_BLOCKING_RESOLVER            (lm_blocking_resolver_get_type ())
#define LM_BLOCKING_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LM_TYPE_BLOCKING_RESOLVER, LmBlockingResolver))
#define LM_BLOCKING_RESOLVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LM_TYPE_BLOCKING_RESOLVER, LmBlockingResolverClass))
#define LM_IS_BLOCKING_RESOLVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LM_TYPE_BLOCKING_RESOLVER))
#define LM_IS_BLOCKING_RESOLVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LM_TYPE_BLOCKING_RESOLVER))
#define LM_BLOCKING_RESOLVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LM_TYPE_BLOCKING_RESOLVER, LmBlockingResolverClass))

typedef struct LmBlockingResolver      LmBlockingResolver;
typedef struct LmBlockingResolverClass LmBlockingResolverClass;

struct LmBlockingResolver {
        LmResolver parent;
};

struct LmBlockingResolverClass {
	LmResolverClass parent_class;
};

GType   lm_blocking_resolver_get_type  (void);

G_END_DECLS

#endif /* __LM_BLOCKING_RESOLVER_H__ */

