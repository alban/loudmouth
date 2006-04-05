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

#ifndef __LM_SSL_INTERNALS_H__
#define __LM_SSL_INTERNALS_H__

#include <glib.h>

LmSSLResponse   _lm_ssl_func_always_continue (LmSSL       *ssl,
					      LmSSLStatus  status,
					      gpointer     user_data);
LmSSL *          _lm_ssl_new              (const gchar    *expected_fingerprint,
					   LmSSLFunction   ssl_function,
					   gpointer        user_data,
					   GDestroyNotify  notify);

void             _lm_ssl_initialize       (LmSSL            *ssl);
gboolean         _lm_ssl_begin            (LmSSL            *ssl,
					   gint              fd,
					   const gchar      *server,
					   GError          **error);
GIOStatus        _lm_ssl_read             (LmSSL            *ssl,
					   gchar            *buf,
					   gint              len,
					   gsize             *bytes_read);
gint             _lm_ssl_send             (LmSSL            *ssl,
					   const gchar      *str,
					   gint              len);
void             _lm_ssl_close            (LmSSL            *ssl);
void             _lm_ssl_free             (LmSSL            *ssl);

LmSSL *          _lm_ssl_new              (const gchar    *expected_fingerprint,
					   LmSSLFunction   ssl_function,
					   gpointer        user_data,
					   GDestroyNotify  notify);

void             _lm_ssl_initialize       (LmSSL            *ssl);
gboolean         _lm_ssl_begin            (LmSSL            *ssl,
					   gint              fd,
					   const gchar      *server,
					   GError          **error);
GIOStatus        _lm_ssl_read             (LmSSL            *ssl,
					   gchar            *buf,
					   gint              len,
					   gsize             *bytes_read);
gint             _lm_ssl_send             (LmSSL            *ssl,
					   const gchar      *str,
					   gint              len);
void             _lm_ssl_close            (LmSSL            *ssl);
void             _lm_ssl_free             (LmSSL            *ssl);

#endif /* __LM_SSL_INTERNALS_H__ */
