/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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

#ifndef __LM_ERROR_H__
#define __LM_ERROR_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

#include <glib.h>

G_BEGIN_DECLS

#define LM_ERROR lm_error_quark ()

typedef enum {
        LM_ERROR_CONNECTION_NOT_OPEN,
        LM_ERROR_CONNECTION_OPEN,
        LM_ERROR_AUTH_FAILED,
	LM_ERROR_CONNECTION_FAILED
} LmError;

GQuark lm_error_quark (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __LM_ERROR_H__ */
