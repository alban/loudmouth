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

#ifndef __LM_OLD_SOCKET_H__ 
#define __LM_OLD_SOCKET_H__

#include <glib.h>

#include "lm-internals.h"

typedef struct _LmOldSocket LmOldSocket;

typedef void    (* IncomingDataFunc)  (LmOldSocket         *socket,
				       const gchar         *buf,
				       gpointer             user_data);

typedef void    (* SocketClosedFunc)  (LmOldSocket         *socket,
				       LmDisconnectReason   reason,
				       gpointer             user_data);

typedef void    (* ConnectResultFunc) (LmOldSocket         *socket,
                                       gboolean             result,
				       gpointer             user_data);

LmOldSocket * lm_old_socket_create          (GMainContext       *context, 
                                             IncomingDataFunc    data_func,
                                             SocketClosedFunc    closed_func,
                                             ConnectResultFunc   connect_func,
                                             gpointer            user_data,
                                             LmConnection       *connection,
                                             gboolean            blocking,
                                             const gchar        *server, 
                                             const gchar        *domain,
                                             guint               port, 
                                             LmSSL              *ssl,
                                             LmProxy            *proxy,
                                             GError           **error);
gint           lm_old_socket_write          (LmOldSocket       *socket,
                                             const gchar       *buf,
                                             gint               len);
void           lm_old_socket_flush          (LmOldSocket        *socket);
void           lm_old_socket_close          (LmOldSocket        *socket);
LmOldSocket *  lm_old_socket_ref            (LmOldSocket        *socket);
void           lm_old_socket_unref          (LmOldSocket        *socket);
gboolean       lm_old_socket_starttls       (LmOldSocket        *socket);
gboolean       lm_old_socket_set_keepalive  (LmOldSocket        *socket, 
                                             int                 delay);
gchar *        lm_old_socket_get_local_host (LmOldSocket        *socket);
void	       lm_old_socket_asyncns_cancel (LmOldSocket        *socket);

gboolean       lm_old_socket_get_use_starttls (LmOldSocket      *socket);
gboolean       lm_old_socket_get_require_starttls (LmOldSocket  *socket);

#endif /* __LM_OLD_SOCKET_H__ */

