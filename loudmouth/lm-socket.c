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
#include "lm-socket.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_DUMMY, LmSocketPriv))

typedef struct LmSocketPriv LmSocketPriv;
struct LmSocketPriv {
	gint my_prop;
};

static void     socket_finalize            (GObject           *object);
static void     socket_get_property        (GObject           *object,
					   guint              param_id,
					   GValue            *value,
					   GParamSpec        *pspec);
static void     socket_set_property        (GObject           *object,
					   guint              param_id,
					   const GValue      *value,
					   GParamSpec        *pspec);

G_DEFINE_TYPE (LmSocket, lm_socket, G_TYPE_OBJECT)

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
lm_socket_class_init (LmSocketClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = socket_finalize;
	object_class->get_property = socket_get_property;
	object_class->set_property = socket_set_property;

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
	
	g_type_class_add_private (object_class, sizeof (LmSocketPriv));
}

static void
lm_socket_init (LmSocket *dummy)
{
	LmSocketPriv *priv;

	priv = GET_PRIV (dummy);

}

static void
socket_finalize (GObject *object)
{
	LmSocketPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (lm_socket_parent_class)->finalize) (object);
}

static void
socket_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	LmSocketPriv *priv;

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
socket_set_property (GObject      *object,
                     guint         param_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
	LmSocketPriv *priv;

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

LmSocket *
lm_socket_new (const gchar *host, guint port)
{
        return NULL;
}

/* Use DNS lookup to find the port and the host */
LmSocket *
lm_socket_new_srv (const gchar *service)
{
        return NULL;
}

void 
lm_socket_connect (LmSocket *socket)
{
        /* Initiate the connection process                 */
        /* DNS lookup, connect thing, create IOchannel etc */
}

gboolean
lm_socket_write (LmSocket *socket,
                 gchar    *data,
                 gsize     len)
{
        return FALSE;
}

gboolean
lm_socket_read (LmSocket *socket,
                gchar    *buf,
                gsize     buf_len,
                gsize     read_len)
{
        return FALSE;
}

void 
lm_socket_disconnect (LmSocket *socket)
{
}


