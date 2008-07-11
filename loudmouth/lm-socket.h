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

#ifndef __LM_SOCKET_H__
#define __LM_SOCKET_H__

#include <glib-object.h>

#include "lm-message.h"
#include "lm-internals.h"

G_BEGIN_DECLS

#define LM_TYPE_SOCKET             (lm_socket_get_type())
#define LM_SOCKET(o)               (G_TYPE_CHECK_INSTANCE_CAST((o), LM_TYPE_SOCKET, LmSocket))
#define LM_IS_SOCKET(o)            (G_TYPE_CHECK_INSTANCE_TYPE((o), LM_TYPE_SOCKET))
#define LM_SOCKET_GET_IFACE(o)     (G_TYPE_INSTANCE_GET_INTERFACE((o), LM_TYPE_SOCKET, LmSocketIface))

typedef struct _LmSocket      LmSocket;
typedef struct _LmSocketIface LmSocketIface;

struct _LmSocketIface {
	GTypeInterface parent;

	/* <vtable> */
        void     (*connect)      (LmSocket *socket);
        gboolean (*write)        (LmSocket *socket,
                                  gchar    *buf,
                                  gsize     len);
        gboolean (*read)         (LmSocket *socket,
                                  gchar    *buf,
                                  gsize     buf_len,
                                  gsize     read_len);
        void     (*disconnect)   (LmSocket *socket);
};

typedef void  (*LmSocketCallback)  (LmSocket *socket,
                                    guint     status_code,
                                    gpointer  user_data);

GType          lm_socket_get_type          (void);

LmSocket *     lm_socket_new               (const gchar *host,
                                            guint        port);
/* Use DNS lookup to find the port and the host */
LmSocket *     lm_socket_new_to_service    (const gchar *service);

/* All async functions so doesn't make a lot of sense to return anything */
/* Use LmSocketCallback instead of signal for the connect result */
void           lm_socket_connect           (LmSocket    *socket);
gboolean       lm_socket_write             (LmSocket    *socket,
                                            gchar       *buf,
                                            gsize        len);
gboolean       lm_socket_read              (LmSocket    *socket,
                                            gchar       *buf,
                                            gsize        buf_len,
                                            gsize        read_len);
void           lm_socket_disconnect        (LmSocket    *socket);

G_END_DECLS

#endif /* __LM_SOCKET_H__ */

