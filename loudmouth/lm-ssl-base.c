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

#include "lm-ssl-base.h"
#include "lm-ssl-internals.h"

void
_lm_ssl_base_init (LmSSLBase      *base, 
		   const gchar    *expected_fingerprint,
		   LmSSLFunction   ssl_function,
		   gpointer        user_data,
		   GDestroyNotify  notify)
{
	base->ref_count      = 1;
	base->func           = ssl_function;
	base->func_data      = user_data;
	base->data_notify    = notify;
	base->fingerprint[0] = '\0';
	
	if (expected_fingerprint) {
		base->expected_fingerprint = g_memdup (expected_fingerprint, 16);
	} else {
		base->expected_fingerprint = NULL;
	}

	if (!base->func) {
		/* If user didn't provide an SSL func the default will be used
		 * this function will always tell the connection to continue.
		 */
		base->func = _lm_ssl_func_always_continue;
	}
}

void
_lm_ssl_base_free_fields (LmSSLBase *base)
{
	g_free (base->expected_fingerprint);
}

