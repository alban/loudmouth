/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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

#ifndef __LM_SASL_H__
#define __LM_SASL_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _LmSASL LmSASL;

typedef void (*LmSASLResultHandler) (LmSASL *sasl,
				     LmConnection *connection,
				     gboolean success,
				     const gchar *reason);

LmSASL *lm_sasl_new (LmConnection *connection);

void lm_sasl_authenticate (LmSASL *sasl, 
			   const gchar *username,
			   const gchar *password,
			   const gchar *server,
			   LmSASLResultHandler handler);

void lm_sasl_free (LmSASL *sasl);

void
lm_sasl_get_auth_params (LmSASL *sasl, const gchar **username,
	const gchar **password);

G_END_DECLS

#endif /* __LM_SASL_H__ */

