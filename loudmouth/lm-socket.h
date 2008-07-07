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

G_BEGIN_DECLS

#define LM_TYPE_DUMMY            (lm_socket_get_type ())
#define LM_SOCKET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LM_TYPE_DUMMY, LmSocket))
#define LM_SOCKET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LM_TYPE_DUMMY, LmSocketClass))
#define LM_IS_DUMMY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LM_TYPE_DUMMY))
#define LM_IS_DUMMY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LM_TYPE_DUMMY))
#define LM_SOCKET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LM_TYPE_DUMMY, LmSocketClass))

typedef struct LmSocket      LmSocket;
typedef struct LmSocketClass LmSocketClass;

struct LmSocket {
	GObject parent;
};

struct LmSocketClass {
	GObjectClass parent_class;
};

GType       lm_socket_get_type      (void);

LmSocket *  lm_socket_new           (const gchar *host,
                                     guint        port);
/* Use DNS lookup to find the port and the host */
LmSocket *  lm_socket_new_srv       (const gchar *service);

/* All async functions so doesn't make a lot of sense to return anything */
void        lm_socket_connect       (LmSocket    *socket);
gboolean    lm_socket_write         (LmSocket    *socket,
                                     gchar       *data,
                                     gsize        len);
gboolean    lm_socket_read          (LmSocket    *socket,
                                     gchar       *buf,
                                     gsize        buf_len,
                                     gsize        read_len);
void        lm_socket_disconnect    (LmSocket    *socket);

G_END_DECLS

#endif /* __LM_SOCKET_H__ */

