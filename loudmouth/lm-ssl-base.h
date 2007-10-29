/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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

#ifndef __LM_SSL_BASE_H__
#define __LM_SSL_BASE_H__

#include "lm-ssl.h"

#define LM_SSL_BASE(x) ((LmSSLBase *) x)

typedef struct _LmSSLBase LmSSLBase;
struct _LmSSLBase {
	LmSSLFunction   func;
	gpointer        func_data;
	GDestroyNotify  data_notify;
	gchar          *expected_fingerprint;
	char            fingerprint[20];
	gboolean        use_starttls;
	gboolean        require_starttls;

	gint            ref_count;
};

void _lm_ssl_base_init         (LmSSLBase      *base, 
				const gchar    *expected_fingerprint,
				LmSSLFunction   ssl_function,
				gpointer        user_data,
				GDestroyNotify  notify);

void _lm_ssl_base_free_fields  (LmSSLBase      *base);

#endif /* __LM_SSL_BASE_H__ */
