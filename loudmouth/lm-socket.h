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

typedef struct _LmSocket LmSocket;

typedef struct {
	/* ConnectCB */
	/* InCB */
	/* HupCB */
} LmSocketFuncs;

typedef enum {
	LM_SOCKET_STATE_CLOSED,
	LM_SOCKET_STATE_DNS_LOOKUP,
	LM_SOCKET_STATE_OPENING,
	LM_SOCKET_STATE_OPEN
} LmSocketState;

LmSocket * lm_socket_new               (LmSocketFuncs     funcs,
					const gchar      *host,
					guint             port);
void       lm_socket_open              (LmSocket         *socket);									
int        lm_socket_get_fd            (LmSocket         *socket);
gboolean   lm_socket_get_is_blocking   (LmSocket         *socket);
void       lm_socket_set_is_blocking   (LmSocket         *socket,
					gboolean          is_block);
int        lm_socket_write             (LmSocket         *socket,
					gsize             size,
					gchar            *buf,
					GError          **error);
int        lm_socket_read              (LmSocket         *socket,
					gsize             size,
					gchar            *buf,
					GError          **error);
gboolean   lm_socket_close             (LmSocket         *socket,
					GError          **error);
LmSock *   lm_socket_ref               (LmSocket         *socket);
void       lm_socket_unref             (LmSocket         *socket);

#endif /* __LM_SOCKET_H__ */

