/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#include <glib.h>

#include "lm-internals.h"

typedef struct _LmSocket LmSocket;

typedef void    (* IncomingDataFunc)  (LmSocket       *socket,
				       const gchar    *buf,
				       gpointer        user_data);

typedef void    (* SocketClosedFunc)  (LmSocket       *socket,
				       LmDisconnectReason reason,
				       gpointer        user_data);

typedef void    (* ConnectResultFunc) (LmSocket        *socket,
				       gboolean         result,
				       gpointer         user_data);

gboolean  lm_socket_output_is_buffered    (LmSocket       *socket,
					   const gchar    *buffer,
					   gint            len);
void      lm_socket_setup_output_buffer   (LmSocket       *socket,
					   const gchar    *buffer,
					   gint            len);
gint      lm_socket_do_write              (LmSocket       *socket,
					   const gchar    *buf,
					   gint            len);

LmSocket *  lm_socket_create              (GMainContext   *context, 
					   IncomingDataFunc data_func,
					   SocketClosedFunc closed_func,
					   ConnectResultFunc connect_func,
					   gpointer         user_data,
					   LmConnection   *connection,
					   gboolean        blocking,
					   const gchar    *server, 
					   const gchar    *domain,
					   guint           port, 
					   LmSSL          *ssl,
					   LmProxy        *proxy,
					   GError        **error);
void        lm_socket_flush               (LmSocket       *socket);
void        lm_socket_close               (LmSocket       *socket);
LmSocket *  lm_socket_ref                 (LmSocket       *socket);
void        lm_socket_unref               (LmSocket       *socket);
#ifdef HAVE_ASYNCNS
void	    _asyncns_cancel               (LmSocket *socket);
#endif
gboolean    lm_socket_starttls            (LmSocket *socket);
gboolean    lm_socket_set_keepalive       (LmSocket *socket, int delay);
gchar *     lm_socket_get_local_host      (LmSocket *socket);

#endif /* __LM_SOCKET_H__ */

