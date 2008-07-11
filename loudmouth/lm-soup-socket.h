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

#ifndef __LM_SOUP_SOCKET_H__
#define __LM_SOUP_SOCKET_H__

#include <glib-object.h>

#include "lm-socket.h"

G_BEGIN_DECLS

#define LM_TYPE_SOUP_SOCKET            (lm_soup_socket_get_type ())
#define LM_SOUP_SOCKET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LM_TYPE_SOUP_SOCKET, LmSoupSocket))
#define LM_SOUP_SOCKET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LM_TYPE_SOUP_SOCKET, LmSoupSocketClass))
#define LM_IS_SOUP_SOCKET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LM_TYPE_SOUP_SOCKET))
#define LM_IS_SOUP_SOCKET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LM_TYPE_SOUP_SOCKET))
#define LM_SOUP_SOCKET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LM_TYPE_SOUP_SOCKET, LmSoupSocketClass))

typedef struct LmSoupSocket      LmSoupSocket;
typedef struct LmSoupSocketClass LmSoupSocketClass;

struct LmSoupSocket {
	GObject parent;
};

struct LmSoupSocketClass {
	GObjectClass parent_class;
	
	/* <vtable> */
	void  (*initialize)    (LmSoupSocket     *soup_socket,
				const char *username,
				const char *server,
				const char *password);
	void  (*begin)         (LmSoupSocket     *soup_socket);
	void  (*cancel)        (LmSoupSocket     *soup_socket);
};

GType   lm_soup_socket_get_type  (void);

G_END_DECLS

#endif /* __LM_SOUP_SOCKET_H__ */

