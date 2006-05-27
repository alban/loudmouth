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

#include <glib.h>

#ifndef __LM_SOCKET_H__
#define __LM_SOCKET_H__

typedef struct _LmSock LmSock;

typedef struct {
	/* ConnectCB */
	/* InCB */
	/* HupCB */
} LmSockFuncs;

typedef enum {
	LM_SOCK_STATE_CLOSED,
	LM_SOCK_STATE_DNS_LOOKUP,
	LM_SOCK_STATE_OPENING,
	LM_SOCK_STATE_OPEN
} LmSockState;

LmSock *   lm_sock_new               (LmSockFuncs  funcs,
 									  const gchar *host,
									  guint        port);
void       lm_sock_open              (LmSock      *sock);									

int        lm_sock_get_fd            (LmSock   *sock);
gboolean   lm_sock_get_is_blocking   (LmSock   *sock);
void       lm_sock_set_is_blocking   (LmSock   *sock,
								      gboolean  is_block);
int        lm_sock_write             (LmSock   *sock,
						              gsize     size,
						 		      gchar    *buf,
						 		      GError  **error);
int        lm_sock_read              (LmSock   *sock,
						 		      gsize     size,
						 		      gchar    *buf,
						 		      GError  **error);
gboolean   lm_sock_close             (LmSock   *sock,
								      GError  **error);
LmSock *   lm_sock_ref               (LmSock   *sock);
void       lm_sock_unref             (LmSock   *sock);

#endif /* __LM_SOCKET_H__ */