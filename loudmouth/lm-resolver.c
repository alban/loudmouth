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
        LmResolverCallback  callback;
        gpointer            user_data;

        /* Properties */
        gchar              *host;
        gchar              *domain;
        gchar              *srv;
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
        PROP_HOST,
        PROP_DOMAIN,
        PROP_SRV
};

enum {
        SIGNAL_NAME,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
lm_resolver_class_init (LmResolverClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = resolver_finalize;
	object_class->get_property = resolver_get_property;
	object_class->set_property = resolver_set_property;

        g_object_class_install_property (object_class,
                                         PROP_HOST,
					 g_param_spec_string ("host",
							      "Host",
							      "Host to lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_DOMAIN,
					 g_param_spec_string ("domain",
							      "Domain",
							      "Domain to lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SRV,
					 g_param_spec_string ("srv",
							      "Srv",
							      "Service to lookup",
                                                              NULL,
                                                              G_PARAM_READWRITE));

	signals[SIGNAL_NAME] = 
		g_signal_new ("signal-name",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      lm_marshal_VOID__INT,
			      G_TYPE_NONE, 
			      1, G_TYPE_INT);
	
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
        case PROP_HOST:
		g_value_set_string (value, priv->host);
		break;
	case PROP_DOMAIN:
		g_value_set_string (value, priv->domain);
		break;
        case PROP_SRV:
		g_value_set_string (value, priv->srv);
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
        case PROP_HOST:
                g_free (priv->host);
		priv->host = g_value_dup_string (value);
		break;
	case PROP_DOMAIN:
                g_free (priv->domain);
		priv->domain = g_value_dup_string (value);
		break;
	case PROP_SRV:
                g_free (priv->srv);
                priv->srv = g_value_dup_string (value);
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
        return g_object_new (LM_TYPE_RESOLVER,
                             "type", LM_RESOLVER_HOST,
                             "host", host,
                             NULL);
}

LmResolver *
lm_resolver_new_for_srv (const gchar        *domain, 
                         const gchar        *srv,
                         LmResolverCallback  callback,
                         gpointer            user_data)
{
        return g_object_new (LM_TYPE_RESOLVER, 
                             "type", LM_RESOLVER_SRV,
                             "domain", domain,
                             "srv", srv,
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

