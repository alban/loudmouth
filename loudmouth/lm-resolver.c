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

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_RESOLVER, LmResolverPriv))

typedef struct LmResolverPriv LmResolverPriv;
struct LmResolverPriv {
        GMainContext       *context;

        LmResolverCallback  callback;
        gpointer            user_data;

        /* -- Properties -- */
        LmResolverType      type;
        gchar              *host;

        /* For SRV lookups */
        gchar              *domain;
        gchar              *service;
        gchar              *protocol;
};

static void     resolver_finalize            (GObject           *object);
static void     resolver_get_property        (GObject           *object,
                                              guint              param_id,
                                              GValue            *value,
                                              GParamSpec        *pspec);
static void     resolver_set_property        (GObject           *object,
                                              guint              param_id,
                                              const GValue      *value,
                                              GParamSpec        *pspec);

G_DEFINE_TYPE (LmResolver, lm_resolver, G_TYPE_OBJECT)

enum {
	PROP_0,
        PROP_CONTEXT,
        PROP_TYPE,
        PROP_HOST,
        PROP_DOMAIN,
        PROP_SERVICE,
        PROP_PROTOCOL
};

static void
lm_resolver_class_init (LmResolverClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = resolver_finalize;
	object_class->get_property = resolver_get_property;
	object_class->set_property = resolver_set_property;

        g_object_class_install_property (object_class,
                                         PROP_CONTEXT,
                                         g_param_spec_pointer ("context",
                                                               "Context",
                                                               "Main context to use",
                                                               G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_TYPE,
					 g_param_spec_int ("type",
                                                           "Type",
                                                           "Resolver Type",
                                                           LM_RESOLVER_HOST,
                                                           LM_RESOLVER_SRV,
                                                           LM_RESOLVER_HOST,
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_HOST,
					 g_param_spec_string ("host",
							      "Host",
							      "Host to lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_DOMAIN,
					 g_param_spec_string ("domain",
							      "Domain",
							      "Domain to lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_SERVICE,
					 g_param_spec_string ("service",
							      "Service",
							      "Service to lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_PROTOCOL,
                                         g_param_spec_string ("protocol",
							      "Protocol",
							      "Protocol for SRV lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (LmResolverPriv));
}

static void
lm_resolver_init (LmResolver *resolver)
{
	LmResolverPriv *priv;

	priv = GET_PRIV (resolver);
}

static void
resolver_finalize (GObject *object)
{
	LmResolverPriv *priv;

	priv = GET_PRIV (object);

        g_free (priv->host);
        g_free (priv->domain);
        g_free (priv->service);
        g_free (priv->protocol);

        if (priv->context) {
                g_main_context_unref (priv->context);
        }

	(G_OBJECT_CLASS (lm_resolver_parent_class)->finalize) (object);
}

static void
resolver_get_property (GObject    *object,
                       guint       param_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
	LmResolverPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
        case PROP_CONTEXT:
                g_value_set_pointer (value, priv->context);
                break;
        case PROP_TYPE:
                g_value_set_int (value, priv->type);
                break;
        case PROP_HOST:
		g_value_set_string (value, priv->host);
		break;
	case PROP_DOMAIN:
		g_value_set_string (value, priv->domain);
		break;
        case PROP_SERVICE:
		g_value_set_string (value, priv->service);
		break;
	case PROP_PROTOCOL:
		g_value_set_string (value, priv->protocol);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
resolver_set_property (GObject      *object,
                       guint         param_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
	LmResolverPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
        case PROP_CONTEXT:
                if (priv->context) {
                        g_main_context_unref (priv->context);
                }

                priv->context = (GMainContext *) g_value_get_pointer (value);
                g_main_context_ref (priv->context);
                break;
        case PROP_TYPE:
                priv->type = g_value_get_int (value);
                break;
        case PROP_HOST:
                g_free (priv->host);
		priv->host = g_value_dup_string (value);
		break;
	case PROP_DOMAIN:
                g_free (priv->domain);
		priv->domain = g_value_dup_string (value);
		break;
        case PROP_SERVICE:
                g_free (priv->service);
                priv->service = g_value_dup_string (value);
		break;
	case PROP_PROTOCOL:
                g_free (priv->protocol);
                priv->protocol = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

LmResolver *
lm_resolver_new_for_host (const gchar        *host,
                          LmResolverCallback  callback,
                          gpointer            user_data)
{
        g_return_val_if_fail (host != NULL, NULL);
        g_return_val_if_fail (callback != NULL, NULL);

        return g_object_new (LM_TYPE_RESOLVER,
                             "type", LM_RESOLVER_HOST,
                             "host", host,
                             NULL);
}

LmResolver *
lm_resolver_new_for_service (const gchar        *domain, 
                             const gchar        *service,
                             const gchar        *protocol,
                             LmResolverCallback  callback,
                             gpointer            user_data)
{
        g_return_val_if_fail (domain != NULL, NULL);
        g_return_val_if_fail (service != NULL, NULL);
        g_return_val_if_fail (protocol != NULL, NULL);
        g_return_val_if_fail (callback != NULL, NULL);

        return g_object_new (LM_TYPE_RESOLVER, 
                             "type", LM_RESOLVER_SRV,
                             "domain", domain,
                             "service", service,
                             "protocol", protocol,
                             NULL);
}

void
lm_resolver_lookup (LmResolver *resolver)
{
        if (!LM_RESOLVER_GET_CLASS(resolver)) {
                g_assert_not_reached ();
        }

        LM_RESOLVER_GET_CLASS(resolver)->lookup (resolver);
}

void
lm_resolver_cancel (LmResolver *resolver)
{
        if (!LM_RESOLVER_GET_CLASS(resolver)->cancel) {
                g_assert_not_reached ();
        }

        LM_RESOLVER_GET_CLASS(resolver)->cancel (resolver);
}

gchar *
lm_resolver_create_srv_string (const gchar *domain, 
                               const gchar *service, 
                               const gchar *protocol) 
{
        g_return_val_if_fail (domain != NULL, NULL);
        g_return_val_if_fail (service != NULL, NULL);
        g_return_val_if_fail (protocol != NULL, NULL);

        return g_strdup_printf ("_%s._%s.%s", service, protocol, domain);
}

