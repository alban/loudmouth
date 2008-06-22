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

#include "lm-xmpp-writer.h"

static void    xmpp_writer_base_init (LmXmppWriterIface *iface);

GType
lm_xmpp_writer_get_type (void)
{
	static GType iface_type = 0;

	if (!iface_type) {
		static const GTypeInfo iface_info = {
			sizeof (LmXmppWriterIface),
			(GBaseInitFunc)     xmpp_writer_base_init,
			(GBaseFinalizeFunc) NULL,
		};

		iface_type = g_type_register_static (G_TYPE_INTERFACE,
						     "LmXmppWriterIface",
						     &iface_info,
						     0);

		g_type_interface_add_prerequisite (iface_type, G_TYPE_OBJECT);
	}

	return iface_type;
}

static void
xmpp_writer_base_init (LmXmppWriterIface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		/* create interface signals here. */
		initialized = TRUE;
	}
}

void
lm_xmpp_writer_send (LmXmppWriter *writer,
                     LmMessage    *message)
{
        if (!LM_XMPP_WRITER_GET_IFACE(writer)->send) {
                g_assert_not_reached ();
	}

	LM_XMPP_WRITER_GET_IFACE(writer)->send (writer, message);
}

void
lm_xmpp_writer_send_text (LmXmppWriter *writer,
                          const gchar  *buf,
                          gsize         len)
{
        if (!LM_XMPP_WRITER_GET_IFACE(writer)->send_text) {
                g_assert_not_reached ();
	}

        LM_XMPP_WRITER_GET_IFACE(writer)->send_text (writer, buf, len);
}

void
lm_xmpp_writer_flush (LmXmppWriter *writer)
{
	if (!LM_XMPP_WRITER_GET_IFACE(writer)->flush) {
		g_assert_not_reached ();
	}

	LM_XMPP_WRITER_GET_IFACE(writer)->flush (writer);
}

