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

#ifndef __LM_SOCK_H__
#define __LM_SOCK_H__

G_BEGIN_DECLS

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may di\sappear or change contents."
#endif

#include <glib.h>

#ifndef G_OS_WIN32

#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#define _LM_SOCK_EINPROGRESS EINPROGRESS
#define _LM_SOCK_EWOULDBLOCK EWOULDBLOOK
#define _LM_SOCK_EALREADY    EALREADY
#define _LM_SOCK_EISCONN     EISCONN
#define _LM_SOCK_EINVAL      EINVAL
#define _LM_SOCK_VALID(S)    ((S) >= 0)

#else  /* G_OS_WIN32 */

/* This means that we require Windows XP or above to build on
 * Windows, the reason for this, is that getaddrinfo() and
 * freeaddrinfo() require ws2tcpip.h functions that are only available
 * on these platforms. 
 *
 * This MUST be defined before windows.h is included.
 */


#if (_WIN32_WINNT < 0x0501)
#undef  WINVER
#define WINVER 0x0501
#undef  _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#define _LM_SOCK_EINPROGRESS WSAEINPROGRESS
#define _LM_SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define _LM_SOCK_EALREADY    WSAEALREADY
#define _LM_SOCK_EISCONN     WSAEISCONN
#define _LM_SOCK_EINVAL      WSAEINVAL
#define _LM_SOCK_VALID(S)    ((S) != INVALID_SOCKET)

#endif /* G_OS_WIN32 */

G_END_DECLS

#endif /* __LM_SOCK__ */
