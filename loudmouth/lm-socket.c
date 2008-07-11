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

static void    socket_base_init (LmSocketIface *iface);

enum {
        READABLE,
        WRITABLE,
        DISCONNECTED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
lm_socket_get_type (void)
{
	static GType iface_type = 0;

	if (!iface_type) {
		static const GTypeInfo iface_info = {
			sizeof (LmSocketIface),
			(GBaseInitFunc)     socket_base_init,
			(GBaseFinalizeFunc) NULL,
		};

		iface_type = g_type_register_static (G_TYPE_INTERFACE,
						     "LmSocketIface",
						     &iface_info,
						     0);

		g_type_interface_add_prerequisite (iface_type, G_TYPE_OBJECT);
	}

	return iface_type;
}

static void
socket_base_init (LmSocketIface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
                signals[READABLE] =
                        g_signal_new ("readable",
                                      LM_TYPE_SOCKET,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      lm_marshal_VOID__BOOLEAN,
                                      G_TYPE_NONE,
                                      1, G_TYPE_BOOLEAN);

                signals[WRITABLE] = 
                        g_signal_new ("writable",
                                      LM_TYPE_SOCKET,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      lm_marshal_VOID__BOOLEAN,
                                      G_TYPE_NONE,
                                      1, G_TYPE_BOOLEAN);
                signals[DISCONNECTED] =
                        g_signal_new ("disconnected",
                                      LM_TYPE_SOCKET,
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL,
                                      lm_marshal_VOID__BOOLEAN,
                                      G_TYPE_NONE,
                                      1, G_TYPE_BOOLEAN);

		initialized = TRUE;
	}
}

LmSocket *
lm_socket_new (const gchar *host, guint port)
{
        g_return_val_if_fail (host != NULL, NULL);

        return NULL;
}

/* Use DNS lookup to find the port and the host */
LmSocket *
lm_socket_new_to_service (const gchar *service)
{
        g_return_val_if_fail (service != NULL, NULL);

        return NULL;
}

void 
lm_socket_connect (LmSocket *socket)
{
        g_return_if_fail (LM_IS_SOCKET (socket));


        /* Initiate the connection process                 */
        /* DNS lookup, connect thing, create IOchannel etc */
        if (!LM_SOCKET_GET_IFACE(socket)->write) {
                g_assert_not_reached ();
        }

        LM_SOCKET_GET_IFACE(socket)->connect (socket);
}

gboolean
lm_socket_write (LmSocket *socket, gchar *buf, gsize len)
{
        g_return_val_if_fail (LM_IS_SOCKET (socket), FALSE);
        g_return_val_if_fail (buf != NULL, FALSE);

        if (!LM_SOCKET_GET_IFACE(socket)->write) {
                g_assert_not_reached ();
        }

        return LM_SOCKET_GET_IFACE(socket)->write (socket, buf, len);
}

gboolean
lm_socket_read (LmSocket *socket,
                gchar    *buf,
                gsize     buf_len,
                gsize     read_len)
{
        g_return_val_if_fail (LM_IS_SOCKET (socket), FALSE);
        g_return_val_if_fail (buf != NULL, FALSE);

        if (!LM_SOCKET_GET_IFACE(socket)->read) {
                g_assert_not_reached ();
        }

        return LM_SOCKET_GET_IFACE(socket)->read (socket, buf, buf_len, read_len);
}

void 
lm_socket_disconnect (LmSocket *socket)
{
        g_return_if_fail (LM_IS_SOCKET (socket));

        if (!LM_SOCKET_GET_IFACE(socket)->disconnect) {
                g_assert_not_reached ();
        }

        LM_SOCKET_GET_IFACE(socket)->disconnect (socket);
}

