/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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
#include "lm-simple-io.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_DUMMY, LmSimpleIOPriv))

typedef struct LmSimpleIOPriv LmSimpleIOPriv;
struct LmSimpleIOPriv {
	gint my_prop;
};

static void     simple_io_finalize            (GObject           *object);
static void     simple_io_get_property        (GObject           *object,
					   guint              param_id,
					   GValue            *value,
					   GParamSpec        *pspec);
static void     simple_io_set_property        (GObject           *object,
					   guint              param_id,
					   const GValue      *value,
					   GParamSpec        *pspec);

G_DEFINE_TYPE (LmSimpleIO, lm_simple_io, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_MY_PROP
};

enum {
	SIGNAL_NAME,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
lm_simple_io_class_init (LmSimpleIOClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = simple_io_finalize;
	object_class->get_property = simple_io_get_property;
	object_class->set_property = simple_io_set_property;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_string ("my-prop",
							      "My Prop",
							      "My Property",
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
	
	g_type_class_add_private (object_class, sizeof (LmSimpleIOPriv));
}

static void
lm_simple_io_init (LmSimpleIO *simple_io)
{
	LmSimpleIOPriv *priv;

	priv = GET_PRIV (simple_io);

}

static void
simple_io_finalize (GObject *object)
{
	LmSimpleIOPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (lm_simple_io_parent_class)->finalize) (object);
}

static void
simple_io_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	LmSimpleIOPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		g_value_set_int (value, priv->my_prop);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
simple_io_set_property (GObject      *object,
		   guint         param_id,
		   const GValue *value,
		   GParamSpec   *pspec)
{
	LmSimpleIOPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		priv->my_prop = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

