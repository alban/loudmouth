/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
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

#ifndef __LM_SSL_H__
#define __LM_SSL_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

G_BEGIN_DECLS

typedef struct _LmSSL LmSSL;
typedef enum {
	LM_CERT_INVALID,
	LM_CERT_ISSUER_NOT_FOUND,
	LM_CERT_REVOKED,
} LmCertificateStatus;

typedef enum {
	LM_SSL_STATUS_NO_CERT_FOUND,	
	LM_SSL_STATUS_UNTRUSTED_CERT,
	LM_SSL_STATUS_CERT_EXPIRED,
	LM_SSL_STATUS_CERT_NOT_ACTIVATED,
	LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH,			
	LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH,			
	LM_SSL_STATUS_GENERIC_ERROR,	
} LmSSLStatus;

typedef enum {
	LM_SSL_RESPONSE_CONTINUE,
	LM_SSL_RESPONSE_STOP,
} LmSSLResponse;

typedef LmSSLResponse (* LmSSLFunction)      (LmSSL        *ssl,
					      LmSSLStatus   status,
					      gpointer      user_data);

LmSSL *               lm_ssl_new             (const gchar *expected_fingerprint,
					      LmSSLFunction   ssl_function,
					      gpointer        user_data,
					      GDestroyNotify  notify);

gboolean              lm_ssl_is_supported    (void);

const gchar *         lm_ssl_get_fingerprint (LmSSL          *ssl);


LmSSL *               lm_ssl_ref             (LmSSL          *ssl);
void                  lm_ssl_unref           (LmSSL          *ssl);

G_END_DECLS

#endif /* __LM_SSL_H__ */
