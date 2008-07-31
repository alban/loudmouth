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

#ifndef __LM_RESOLVER_H__
#define __LM_RESOLVER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LM_TYPE_RESOLVER            (lm_resolver_get_type ())
#define LM_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LM_TYPE_RESOLVER, LmResolver))
#define LM_RESOLVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LM_TYPE_RESOLVER, LmResolverClass))
#define LM_IS_RESOLVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LM_TYPE_RESOLVER))
#define LM_IS_RESOLVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LM_TYPE_RESOLVER))
#define LM_RESOLVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LM_TYPE_RESOLVER, LmResolverClass))

typedef struct LmResolver      LmResolver;
typedef struct LmResolverClass LmResolverClass;

struct LmResolver {
	GObject parent;
};

struct LmResolverClass {
	GObjectClass parent_class;
	
	/* <vtable> */
        void (*lookup)  (LmResolver  *resolver);
        void (*cancel)  (LmResolver  *resolver);
};

typedef enum {
        LM_RESOLVER_HOST,
        LM_RESOLVER_SRV
} LmResolverType;

typedef enum {
        LM_RESOLVER_RESULT_OK,
        LM_RESOLVER_RESULT_FAILED
} LmResolverResult;

typedef void (*LmResolverCallback) (LmResolver       *resolver,
                                    LmResolverResult  result,
                                    gpointer          user_data);

GType             lm_resolver_get_type          (void);
LmResolver *      lm_resolver_new_for_host      (const gchar        *host,
                                                 LmResolverCallback  callback,
                                                 gpointer            user_data);
LmResolver *      lm_resolver_new_for_service   (const gchar        *domain,
                                                 const gchar        *service,
                                                 const gchar        *protocol,
                                                 LmResolverCallback  callback,
                                                 gpointer            user_data);
void              lm_resolver_lookup            (LmResolver         *resolver);
void              lm_resolver_cancel            (LmResolver         *resolver);
gchar *           lm_resolver_create_srv_string (const gchar        *domain, 
                                                 const gchar        *service,
                                                 const gchar        *protocol);

/* To iterate through the results */ 
struct addrinfo * lm_resolver_results_get_next  (LmResolver         *resolver);
void              lm_resolver_results_reset     (LmResolver         *resolver);

/* Only for sub classes */
void              _lm_resolver_set_result       (LmResolver         *resolver,
                                                 LmResolverResult    result,
                                                 struct addrinfo    *results);

G_END_DECLS

#endif /* __LM_RESOLVER_H__ */

