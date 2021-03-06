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

#include <libsoup/soup.h>

#include "lm-marshal.h"
#include "lm-soup-socket.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_SOUP_SOCKET, LmSoupSocketPriv))

typedef struct LmSoupSocketPriv LmSoupSocketPriv;
struct LmSoupSocketPriv {
        gint          my_prop;
        SoupSocket   *soup;
        GCancellable *cancellable;
};

static void     soup_socket_iface_init          (LmSocketIface     *iface);
static void     soup_socket_finalize            (GObject           *object);
static void     soup_socket_get_property        (GObject           *object,
                                                 guint              param_id,
                                                 GValue            *value,
                                                 GParamSpec        *pspec);
static void     soup_socket_set_property        (GObject           *object,
                                                 guint              param_id,
                                                 const GValue      *value,
                                                 GParamSpec        *pspec);
/* Loudmouths naming policy for static functions clash with the naming of 
 * Libsoups public functions here, hence the _ */
static void     _soup_socket_connect            (LmSocket          *socket);
static gboolean _soup_socket_write              (LmSocket          *socket,
                                                 gchar             *data, 
                                                 gsize              len);
static gboolean _soup_socket_read               (LmSocket          *socket,
                                                 gchar             *buf,
                                                 gsize              buf_len,
                                                 gsize             *read_len);
static void     _soup_socket_disconnect         (LmSocket          *socket);
static void     _soup_socket_callback           (SoupSocket        *soup,
                                                 guint              status,
                                                 LmSocket          *socket);

G_DEFINE_TYPE_WITH_CODE (LmSoupSocket, lm_soup_socket, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (LM_TYPE_SOCKET,
                                                soup_socket_iface_init))

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
lm_soup_socket_class_init (LmSoupSocketClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = soup_socket_finalize;
	object_class->get_property = soup_socket_get_property;
	object_class->set_property = soup_socket_set_property;

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
	
	g_type_class_add_private (object_class, sizeof (LmSoupSocketPriv));
}

static void
lm_soup_socket_init (LmSoupSocket *soup_socket)
{
	LmSoupSocketPriv *priv;

	priv = GET_PRIV (soup_socket);
}

static void
soup_socket_iface_init (LmSocketIface *iface)
{
        iface->connect    = _soup_socket_connect;
        iface->write      = _soup_socket_write;
        iface->read       = _soup_socket_read;
        iface->disconnect = _soup_socket_disconnect;
}

static void
soup_socket_finalize (GObject *object)
{
	LmSoupSocketPriv *priv;

	priv = GET_PRIV (object);

        if (priv->soup) {
                if (soup_socket_is_connected (priv->soup)) {
                        soup_socket_disconnect (priv->soup);
                }

                g_object_unref (priv->soup);
        }

	(G_OBJECT_CLASS (lm_soup_socket_parent_class)->finalize) (object);
}

static void
soup_socket_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
	LmSoupSocketPriv *priv;

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
soup_socket_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
	LmSoupSocketPriv *priv;

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

static void
_soup_socket_connect (LmSocket *socket)
{
        LmSoupSocketPriv *priv = GET_PRIV (socket);

        if (priv->soup) {
                g_warning ("Already have a SoupSocket");
                return;
        }

        priv->soup = soup_socket_new ("remote-address", "kenny.imendio.com",
                                      "non-blocking", TRUE,
                                      NULL);

        soup_socket_connect_async (priv->soup,
                                   priv->cancellable,
                                   (SoupSocketCallback)_soup_socket_callback,
                                   socket);
}

static gboolean
_soup_socket_write (LmSocket *socket,
                   gchar    *data, 
                   gsize     len)
{
        LmSoupSocketPriv   *priv = GET_PRIV (socket);
        SoupSocketIOStatus  io_status;

        if (!priv->soup || !soup_socket_is_connected (priv->soup)) {
                g_warning ("Trying to write to an unconnected socket");
                return FALSE;
        }

        io_status = soup_socket_write (priv->soup,
                                       data, len,
                                       NULL /* FIXME: Should be how much was written */,
                                       priv->cancellable,
                                       NULL /* FIXME: Should be a GError */);
        return TRUE; /* FIXME: Return properly depending on IOStatus */
}

static gboolean
_soup_socket_read (LmSocket *socket,
                  gchar     *buf,
                  gsize      buf_len,
                  gsize     *read_len)
{
        LmSoupSocketPriv *priv = GET_PRIV (socket);
        SoupSocketIOStatus  io_status;

        if (!priv->soup || !soup_socket_is_connected (priv->soup)) {
                g_warning ("Trying to read from an unconnected socket");
                return FALSE;
        }

        io_status = soup_socket_read (priv->soup,
                                      buf, buf_len, read_len,
                                      priv->cancellable,
                                      NULL /* FIXME: Look at if the match is up tonight */);


        return FALSE;
}

static void
_soup_socket_disconnect (LmSocket *socket)
{
        LmSoupSocketPriv *priv = GET_PRIV (socket);

        if (priv->soup) {
                soup_socket_disconnect (priv->soup);
        }
}

static void
_soup_socket_callback (SoupSocket *soup,
                       guint       status,
                       LmSocket   *socket)
{
        /* Signal back to caller of lm_socket_connect through LmSocketCallback */
}

