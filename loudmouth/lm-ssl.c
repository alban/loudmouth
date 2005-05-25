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

#include <config.h>

#include "lm-internals.h"

#include <string.h>
#include <glib.h>

#include "lm-error.h"

#ifdef HAVE_GNUTLS
#include <gnutls/x509.h>
#endif

struct _LmSSL {
	LmSSLFunction   func;
	gpointer        func_data;
	GDestroyNotify  data_notify;
	gchar          *expected_fingerprint;
	char            fingerprint[20];

	gint            ref_count;
#ifdef HAVE_GNUTLS
	gnutls_session  gnutls_session;
	gnutls_certificate_client_credentials gnutls_xcred;
#endif
};

static void      ssl_free                (LmSSL       *ssl);

#ifdef HAVE_GNUTLS
static gboolean  ssl_verify_certificate  (LmSSL       *ssl,
					  const gchar *server);

static gboolean
ssl_verify_certificate (LmSSL *ssl, const gchar *server)
{
	int           status;

	/* This verification function uses the trusted CAs in the credentials
	 * structure. So you must have installed one or more CA certificates.
	 */
	status = gnutls_certificate_verify_peers (ssl->gnutls_session);

	if (status == GNUTLS_E_NO_CERTIFICATE_FOUND) {
		if (ssl->func (ssl,
			       LM_SSL_STATUS_NO_CERT_FOUND,
			       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
			return FALSE;
		}
	}
	
	if (status & GNUTLS_CERT_INVALID
	    || status & GNUTLS_CERT_REVOKED) {
		if (ssl->func (ssl, LM_SSL_STATUS_UNTRUSTED_CERT,
			       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
			return FALSE;
		}
	}
	
	if (gnutls_certificate_expiration_time_peers (ssl->gnutls_session) < time (0)) {
		if (ssl->func (ssl, LM_SSL_STATUS_CERT_EXPIRED,
			       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
			return FALSE;
		}
	}
	
	if (gnutls_certificate_activation_time_peers (ssl->gnutls_session) > time (0)) {
		if (ssl->func (ssl, LM_SSL_STATUS_CERT_NOT_ACTIVATED,
			       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
			return FALSE;
		}
	}
	
	if (gnutls_certificate_type_get (ssl->gnutls_session) == GNUTLS_CRT_X509) {
		const gnutls_datum* cert_list;
		guint cert_list_size;
		size_t digest_size;
		gnutls_x509_crt cert;
		
		cert_list = gnutls_certificate_get_peers (ssl->gnutls_session, &cert_list_size);
		if (cert_list == NULL) {
			if (ssl->func (ssl, LM_SSL_STATUS_NO_CERT_FOUND,
				       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				return FALSE;
			}
		}

		gnutls_x509_crt_init (&cert);

		if (!gnutls_x509_crt_import (cert, &cert_list[0],
					     GNUTLS_X509_FMT_DER)) {
			if (ssl->func (ssl, LM_SSL_STATUS_NO_CERT_FOUND, 
				       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				return FALSE;
			}
		}
		
		if (!gnutls_x509_crt_check_hostname (cert, server)) {
			if (ssl->func (ssl, LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH,
				       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				return FALSE;
			}
		}

		gnutls_x509_crt_deinit (cert);

		if (gnutls_fingerprint (GNUTLS_DIG_MD5, &cert_list[0],
					     ssl->fingerprint,
					     &digest_size) >= 0) {
			if (ssl->expected_fingerprint &&
			    memcmp (ssl->expected_fingerprint, ssl->fingerprint,
				    digest_size) &&
			    ssl->func (ssl,
				       LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH,
				       ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				return FALSE;
			}
		} 
		else if (ssl->func (ssl, LM_SSL_STATUS_GENERIC_ERROR,
				    ssl->func_data) != LM_SSL_RESPONSE_CONTINUE) {
			return FALSE; 
		} 
	}

	return TRUE;
}

void
_lm_ssl_initialize (LmSSL *ssl) 
{
	gnutls_global_init ();
	gnutls_certificate_allocate_credentials (&ssl->gnutls_xcred);
}

gboolean
_lm_ssl_begin (LmSSL *ssl, gint fd, const gchar *server, GError **error)
{
	int ret;
	gboolean auth_ok = TRUE;
	const int cert_type_priority[2] =
	{ GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP };

	gnutls_init (&ssl->gnutls_session, GNUTLS_CLIENT);
	gnutls_set_default_priority (ssl->gnutls_session);
	gnutls_certificate_type_set_priority (ssl->gnutls_session,
					      cert_type_priority);
	gnutls_credentials_set (ssl->gnutls_session,
				GNUTLS_CRD_CERTIFICATE,
				ssl->gnutls_xcred);

	gnutls_transport_set_ptr (ssl->gnutls_session,
				  (gnutls_transport_ptr) fd);

	ret = gnutls_handshake (ssl->gnutls_session);

	if (ret >= 0) {
		auth_ok = ssl_verify_certificate (ssl, server);
	}

	if (ret < 0 || !auth_ok) {
		char *errmsg;

		gnutls_perror (ret);
	
		if (!auth_ok) {
			errmsg = "*** GNUTLS authentication error";
		} else {
			errmsg = "*** GNUTLS handshake failed";
		}

		g_set_error (error, 
			     LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			     errmsg);			

		return FALSE;
	}
	
	return TRUE;
}

GIOStatus
_lm_ssl_read (LmSSL *ssl, gchar *buf, gint len, gsize *bytes_read)
{
	GIOStatus status;
	gint      b_read;

	*bytes_read = 0;
	b_read = gnutls_record_recv (ssl->gnutls_session, buf, len);

	if (b_read == GNUTLS_E_AGAIN) {
		status = G_IO_STATUS_AGAIN;
	}
	else if (b_read > len) {
		status = G_IO_STATUS_EOF;
	}
	else if (b_read < 0) {
		status = G_IO_STATUS_ERROR;
	} else {
		*bytes_read = (guint) b_read;
		status = G_IO_STATUS_NORMAL;
	}

	return status;
}

gboolean
_lm_ssl_send (LmSSL *ssl, const gchar *str, gint len)
{
	gint bytes_written;

	bytes_written = gnutls_record_send (ssl->gnutls_session, str, len);

	while (bytes_written < 0) {
		if (bytes_written != GNUTLS_E_INTERRUPTED &&
		    bytes_written != GNUTLS_E_AGAIN) {
			return FALSE;
		}
	
		bytes_written = gnutls_record_send (ssl->gnutls_session, 
						    str, len);
	}

	return TRUE;
}

void 
_lm_ssl_close (LmSSL *ssl)
{
	gnutls_deinit (ssl->gnutls_session);
	gnutls_certificate_free_credentials (ssl->gnutls_xcred);
	gnutls_global_deinit ();
}
#endif 


static void
ssl_free (LmSSL *ssl)
{
	g_free (ssl->expected_fingerprint);
	g_free (ssl);
}

/**
 * lm_ssl_is_supported:
 *
 * Checks whether Loudmouth supports SSL or not.
 *
 * Return value: #TRUE if this installation of Loudmouth supports SSL, otherwise returns #FALSE.
 **/
gboolean
lm_ssl_is_supported (void)
{
#ifdef HAVE_GNUTLS
	return TRUE;
#else
	return FALSE;
#endif
}

/**
 * lm_ssl_new:
 * @expected_fingerprint: The expected fingerprint. @ssl_function will be called if there is a mismatch. %NULL if you are not interested in this check.
 * @ssl_function: Callback called to inform the user of a problem during setting up the SSL connection and how to proceed.
 * @user_data: Data sent with the callback.
 * @notify: Function to free @user_dataa when the connection is finished. %NULL if @user_data should not be freed.
 *
 * Creates a new SSL struct, call #lm_connection_set_ssl to use it. 
 *
 * Return value: A new #LmSSL struct.
 **/
LmSSL *
lm_ssl_new (const gchar    *expected_fingerprint,
	    LmSSLFunction   ssl_function,
	    gpointer        user_data,
	    GDestroyNotify  notify)
{
	LmSSL *ssl;

	ssl = g_new0 (LmSSL, 1);
	
	ssl->ref_count      = 1;
	ssl->func           = ssl_function;
	ssl->func_data      = user_data;
	ssl->data_notify    = notify;
	ssl->fingerprint[0] = '\0';

	if (expected_fingerprint) {
		ssl->expected_fingerprint = g_strdup (expected_fingerprint);
	} else {
		ssl->expected_fingerprint = NULL;
	}

	return ssl;
}

/**
 * lm_ssl_get_fingerprint: 
 * @ssl: an #LmSSL
 *
 * Returns the MD5 fingerprint of the remote server's certificate.
 * 
 * Return value: A 16-byte array representing the fingerprint or %NULL if unknown.
 **/
const gchar *
lm_ssl_get_fingerprint (LmSSL *ssl)
{
	g_return_val_if_fail (ssl != NULL, NULL);
	
	return ssl->fingerprint;
}

/**
 * lm_ssl_ref:
 * @ssl: an #LmSSL
 * 
 * Adds a reference to @ssl.
 * 
 * Return value: the ssl
 **/
LmSSL *
lm_ssl_ref (LmSSL *ssl)
{
	g_return_val_if_fail (ssl != NULL, NULL);

	ssl->ref_count++;

	return ssl;
}

/**
 * lm_ssl_unref
 * @ssl: an #LmSSL
 * 
 * Removes a reference from @ssl. When no more references are present
 * @ssl is freed.
 **/
void 
lm_ssl_unref (LmSSL *ssl)
{
	g_return_if_fail (ssl != NULL);
        
        ssl->ref_count --;
        
        if (ssl->ref_count == 0) {
		if (ssl->data_notify) {
			(* ssl->data_notify) (ssl->func_data);
		}
               
		ssl_free (ssl);
        }
}

/* Define the GnuTLS functions as noops if we compile without support */
#ifndef HAVE_GNUTLS

void
_lm_ssl_initialize (LmSSL *ssl)
{
	/* NOOP */
}

gboolean
_lm_ssl_begin (LmSSL        *ssl,
	       gint          fd,
	       const gchar  *server,
	       GError      **error)
{
	return TRUE;
}

GIOStatus
_lm_ssl_read (LmSSL *ssl,
	      gchar *buf,
	      gint   len,
	      gsize  *bytes_read)
{
	/* NOOP */
	*bytes_read = 0;

	return G_IO_STATUS_EOF;
}

gboolean 
_lm_ssl_send (LmSSL *ssl, const gchar *str, gint len)
{
	/* NOOP */
	return TRUE;
}
void 
_lm_ssl_close (LmSSL *ssl)
{
	/* NOOP */
}

#endif

