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

#ifndef __LM_XMPP_WRITER_H__
#define __LM_XMPP_WRITER_H__

#include <glib-object.h>

#include "lm-message.h"
#include "lm-socket.h"

G_BEGIN_DECLS

#define LM_TYPE_XMPP_WRITER             (lm_xmpp_writer_get_type())
#define LM_XMPP_WRITER(o)               (G_TYPE_CHECK_INSTANCE_CAST((o), LM_TYPE_XMPP_WRITER, LmXmppWriter))
#define LM_IS_XMPP_WRITER(o)            (G_TYPE_CHECK_INSTANCE_TYPE((o), LM_TYPE_XMPP_WRITER))
#define LM_XMPP_WRITER_GET_IFACE(o)     (G_TYPE_INSTANCE_GET_INTERFACE((o), LM_TYPE_XMPP_WRITER, LmXmppWriterIface))

typedef struct _LmXmppWriter      LmXmppWriter;
typedef struct _LmXmppWriterIface LmXmppWriterIface;

struct _LmXmppWriterIface {
	GTypeInterface parent;

	/* <vtable> */
        void (*send_message) (LmXmppWriter *writer,
                              LmMessage    *message);
        void (*send_text)    (LmXmppWriter *writer,
                              const gchar  *buf,
                              gsize         len);

        /* Needed? */
        void (*flush)        (LmXmppWriter   *writer);
};

GType          lm_xmpp_writer_get_type      (void);

LmXmppWriter * lm_xmpp_writer_new           (LmSocket       *socket);

void           lm_xmpp_writer_send_message  (LmXmppWriter   *writer,
                                             LmMessage      *message);
void           lm_xmpp_writer_send_text     (LmXmppWriter   *writer,
                                             const gchar    *buf,
                                             gsize           len);
void           lm_xmpp_writer_flush         (LmXmppWriter   *writer);

G_END_DECLS

#endif /* __LM_XMPP_WRITER_H__ */

