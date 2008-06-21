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

/**
 * SECTION:lm-ssl
 * @Short_description: SSL struct for SSL support in Loudmouth
 * 
 * Use this together with an #LmConnection to get the connection to use SSL. Example of how to use the #LmSSL API.
 * 
 * <informalexample><programlisting><![CDATA[
 * LmConnection *connection;
 * LmSSL        *ssl;
 * 
 * connection = lm_connection_new ("myserver");
 * ssl = lm_ssl_new (NULL, my_ssl_func, NULL, NULL);
 * lm_connection_set_ssl (connection, ssl);
 * ...
 * ]]></programlisting></informalexample>
 */

#ifndef __LM_SSL_H__
#define __LM_SSL_H__

#include <glib.h>

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

G_BEGIN_DECLS

/**
 * LmSSL:
 * 
 * This should not be accessed directly. Use the accessor functions as described below.
 */
typedef struct _LmSSL LmSSL;

/**
 * LmCertificateStatus:
 * @LM_CERT_INVALID: The certificate is invalid.
 * @LM_CERT_ISSUER_NOT_FOUND: The issuer of the certificate is not found.
 * @LM_CERT_REVOKED: The certificate has been revoked.
 * 
 * Provides information of the status of a certain certificate.
 */
typedef enum {
	LM_CERT_INVALID,
	LM_CERT_ISSUER_NOT_FOUND,
	LM_CERT_REVOKED
} LmCertificateStatus;

/**
 * LmSSLStatus:
 * @LM_SSL_STATUS_NO_CERT_FOUND: The server doesn't provide a certificate.
 * @LM_SSL_STATUS_UNTRUSTED_CERT: The certification can not be trusted.
 * @LM_SSL_STATUS_CERT_EXPIRED: The certificate has expired.
 * @LM_SSL_STATUS_CERT_NOT_ACTIVATED: The certificate has not been activated.
 * @LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH: The server hostname doesn't match the one in the certificate.
 * @LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH: The fingerprint doesn't match your expected.
 * @LM_SSL_STATUS_GENERIC_ERROR: Some other error.
 * 
 * Provides information about something gone wrong when trying to setup the SSL connection.
 */
typedef enum {
	LM_SSL_STATUS_NO_CERT_FOUND,	
	LM_SSL_STATUS_UNTRUSTED_CERT,
	LM_SSL_STATUS_CERT_EXPIRED,
	LM_SSL_STATUS_CERT_NOT_ACTIVATED,
	LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH,			
	LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH,			
	LM_SSL_STATUS_GENERIC_ERROR
} LmSSLStatus;

/**
 * LmSSLResponse:
 * @LM_SSL_RESPONSE_CONTINUE: Continue to connect.
 * @LM_SSL_RESPONSE_STOP: Stop the connection.
 * 
 * Used to inform #LmConnection if you want to stop due to an error reported or if you want to continue to connect.
 */
typedef enum {
	LM_SSL_RESPONSE_CONTINUE,
	LM_SSL_RESPONSE_STOP
} LmSSLResponse;

/**
 * LmSSLFunction:
 * @ssl: An #LmSSL.
 * @status: The status informing what went wrong.
 * @user_data: User data provided in the callback.
 * 
 * This function is called if something goes wrong during the connecting phase.
 * 
 * Returns: User should return #LM_SSL_RESPONSE_CONTINUE if connection should proceed and otherwise #LM_SSL_RESPONSE_STOP.
 */
typedef LmSSLResponse (* LmSSLFunction)      (LmSSL        *ssl,
					      LmSSLStatus   status,
					      gpointer      user_data);

LmSSL *               lm_ssl_new             (const gchar *expected_fingerprint,
					      LmSSLFunction   ssl_function,
					      gpointer        user_data,
					      GDestroyNotify  notify);

gboolean              lm_ssl_is_supported    (void);

const gchar *         lm_ssl_get_fingerprint (LmSSL          *ssl);

void                  lm_ssl_use_starttls    (LmSSL *ssl,
					      gboolean use_starttls,
					      gboolean require);

gboolean              lm_ssl_get_use_starttls (LmSSL *ssl);

gboolean              lm_ssl_get_require_starttls (LmSSL *ssl);

LmSSL *               lm_ssl_ref             (LmSSL          *ssl);
void                  lm_ssl_unref           (LmSSL          *ssl);

G_END_DECLS

#endif /* __LM_SSL_H__ */
