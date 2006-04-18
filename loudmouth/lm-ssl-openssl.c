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
	LmSSLBase base;

	SSL_CTX *ctx;

	SSL     *session;
/*	gnutls_certificate_client_credentials gnutls_xcred;*/
};

static gboolean       ssl_verify_certificate    (LmSSL       *ssl,
						 const gchar *server);
static GIOStatus      ssl_io_status_from_return (LmSSL       *ssl,
						 gint         error);

static gboolean
ssl_verify_certificate (LmSSL *ssl, const gchar *server)
{
	LmSSLBase *base;

	base = LM_SSL_BASE (ssl);

	/* FIXME: Implement */

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

	ssl->session = SSL_new (ssl->ctx);
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
