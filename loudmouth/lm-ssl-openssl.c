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

#include <config.h>

#include <string.h>
#include <glib.h>

#include "lm-error.h"
#include "lm-ssl-base.h"
#include "lm-ssl-internals.h"

#ifdef HAVE_OPENSSL

#include <openssl/ssl.h>

struct _LmSSL {
	LmSSLBase  base;

	SSL_CTX   *ctx;

	SSL       *session;
/*	gnutls_certificate_client_credentials gnutls_xcred;*/
};

static gboolean       ssl_verify_certificate    (LmSSL       *ssl,
						 const gchar *server);
static GIOStatus      ssl_io_status_from_return (LmSSL       *ssl,
						 gint         error);

static gboolean
ssl_verify_certificate (LmSSL *ssl, const gchar *server)
{
	LmSSLBase   *base;
	int          result;
	LmSSLStatus  status;

	base = LM_SSL_BASE (ssl);

	result = SSL_get_verify_result (ssl->session);

	/* Result values from 'man verify' */
	switch (result) {
	case X509_V_OK:
		return TRUE;
	case X509_V_ERR_CERT_HAS_EXPIRED:
		status = LM_SSL_STATUS_CERT_EXPIRED;
		break;
	case X509_V_ERR_CERT_NOT_YET_VALID:
		status = LM_SSL_STATUS_CERT_NOT_ACTIVATED;
		break;
	case X509_V_ERR_CERT_UNTRUSTED:
		status = LM_SSL_STATUS_UNTRUSTED_CERT;
		break;
	case X509_V_ERR_CERT_REVOKED:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	case X509_V_ERR_UNABLE_TO_GET_CRL:
	case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
	case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
	case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
	case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
	case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
	case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
	case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
	case X509_V_ERR_OUT_OF_MEM:
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	case X509_V_ERR_APPLICATION_VERIFICATION:
	case X509_V_ERR_CERT_CHAIN_TOO_LONG:
	case X509_V_ERR_CERT_SIGNATURE_FAILURE:
	case X509_V_ERR_CRL_SIGNATURE_FAILURE:
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
	case X509_V_ERR_INVALID_CA:
	case X509_V_ERR_PATH_LENGTH_EXCEEDED:
	case X509_V_ERR_INVALID_PURPOSE:
	case X509_V_ERR_CERT_REJECTED:
	case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
	case X509_V_ERR_AKID_SKID_MISMATCH:
	case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
	case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
		/* FIXME: These doesn't map very well to LmSSLStatus right 
		 *        now. */
		status = LM_SSL_STATUS_GENERIC_ERROR;
		break;
	default:
		status = LM_SSL_STATUS_GENERIC_ERROR;
		g_warning ("Unmatched error code '%d' from SSL_get_verify_result", result);
		break;
	};

	if (base->func (ssl, status, base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
		return FALSE;
	}

	return TRUE;
}

static GIOStatus
ssl_io_status_from_return (LmSSL *ssl, gint ret)
{
	gint      error;
	GIOStatus status;

	if (ret > 0) {
		return G_IO_STATUS_NORMAL;
	}

	error = SSL_get_error (ssl->session, ret);

	if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
		status = G_IO_STATUS_AGAIN;
	} 
	else if (error == SSL_ERROR_ZERO_RETURN) {
		status = G_IO_STATUS_EOF;
	} else {
		status = G_IO_STATUS_ERROR;
	}

	return status;
}

/* From lm-ssl-protected.h */

LmSSL *
_lm_ssl_new (const gchar    *expected_fingerprint,
	    LmSSLFunction   ssl_function,
	    gpointer        user_data,
	    GDestroyNotify  notify)
{
	LmSSL *ssl;

	ssl = g_new0 (LmSSL, 1);

	_lm_ssl_base_init ((LmSSLBase *) ssl,
			   expected_fingerprint,
			   ssl_function, user_data, notify);

	return ssl;
}

void
_lm_ssl_initialize (LmSSL *ssl) 
{
	static gboolean  initialized = FALSE;
	SSL_METHOD      *meth;

	if (!initialized) {
		SSL_library_init ();

		/* FIXME: Is this needed when we are not in debug? */
		SSL_load_error_strings ();
		initialized = TRUE;
	}

	meth = SSLv23_method ();
	ssl->ctx = SSL_CTX_new (meth);
}

gboolean
_lm_ssl_begin (LmSSL *ssl, gint fd, const gchar *server, GError **error)
{
	BIO       *sbio;
	GIOStatus  status;

	if (!ssl->ctx) {
		g_set_error (error,
			     LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			     "No SSL Context for OpenSSL");
		return FALSE;
	}

	ssl->session = SSL_new (ssl->ctx);
	if (ssl->session == NULL) {
		g_set_error (error,
			     LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			     "Failed to create an SSL session through OpenSSL");
		return FALSE;
	}
	
	sbio = BIO_new_socket (fd, BIO_NOCLOSE);
	SSL_set_bio (ssl->session, sbio, sbio);

	while (TRUE) {
		gint ret;

		ret = SSL_connect (ssl->session);

		if (ret > 0) {
			/* Successful */
			break;
		}
		else {
			status = ssl_io_status_from_return (ssl, ret);
			if (status == G_IO_STATUS_AGAIN) {
				/* Try again */
				continue;
			} else {
				g_set_error (error, 
					     LM_ERROR, LM_ERROR_CONNECTION_OPEN,
					     "*** OpenSSL handshake failed");
				return FALSE;
			}

		}
	}

	if (!ssl_verify_certificate (ssl, server)) {
		g_set_error (error,
			     LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			     "*** OpenSSL certificate verification failed");
		return FALSE;
	}

	/* FIXME: Check creds */
	return TRUE; 
}

GIOStatus
_lm_ssl_read (LmSSL *ssl, gchar *buf, gint len, gsize *bytes_read)
{
	GIOStatus status;
	gint      b_read;

	*bytes_read = 0;
	b_read = SSL_read (ssl->session, buf, len);

	status = ssl_io_status_from_return (ssl, b_read);

	if (status == G_IO_STATUS_NORMAL) {
		*bytes_read = b_read;
	}

	return status;
}

gint
_lm_ssl_send (LmSSL *ssl, const gchar *str, gint len)
{
	GIOStatus status;
	gint      bytes_written;


	bytes_written = SSL_write (ssl->session, str, len);
	status = ssl_io_status_from_return (ssl, bytes_written);
	
	while (bytes_written < 0) {
		if (status != G_IO_STATUS_AGAIN) {
			return -1;
		}

		bytes_written = SSL_write (ssl->session, str, len);
		status = ssl_io_status_from_return (ssl, bytes_written);
	}

	return bytes_written;
}

void 
_lm_ssl_close (LmSSL *ssl)
{
	SSL_free (ssl->session);
	ssl->session = NULL;
}

void
_lm_ssl_free (LmSSL *ssl)
{
	_lm_ssl_base_free_fields (LM_SSL_BASE (ssl));

	SSL_CTX_free (ssl->ctx);

	g_free (ssl);
}

#endif /* HAVE_GNUTLS */
