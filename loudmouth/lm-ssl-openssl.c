/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
 * Copyright (C) 2006 Nokia Corporation. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include "lm-error.h"
#include "lm-ssl-base.h"
#include "lm-ssl-internals.h"

#ifdef HAVE_OPENSSL

#include <openssl/ssl.h>
#include <openssl/err.h>

#define LM_SSL_CN_MAX		63

struct _LmSSL {
	LmSSLBase base;

	SSL_METHOD *ssl_method;
	SSL_CTX *ssl_ctx;
	SSL *ssl;
	/*BIO *bio;*/
};

int ssl_verify_cb (int preverify_ok, X509_STORE_CTX *x509_ctx);

static gboolean ssl_verify_certificate (LmSSL *ssl, const gchar *server);
static GIOStatus ssl_io_status_from_return (LmSSL *ssl, gint error);

/*static char _ssl_error_code[11];*/

static void
ssl_print_state (LmSSL *ssl, const char *func, int val)
{
	unsigned long errid;
	const char *errmsg;

	switch (SSL_get_error(ssl->ssl, val)) {
		case SSL_ERROR_NONE:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_NONE\n",
				func, val);
			break;
		case SSL_ERROR_ZERO_RETURN:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_ZERO_RETURN\n",
				func, val);
			break;
		case SSL_ERROR_WANT_READ:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_WANT_READ\n",
				func, val);
			break;
		case SSL_ERROR_WANT_WRITE:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_WANT_WRITE\n",
				func, val);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_WANT_X509_LOOKUP\n",
				func, val);
			break;
		case SSL_ERROR_SYSCALL:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_SYSCALL\n",
				func, val);
			break;
		case SSL_ERROR_SSL:
			fprintf(stderr,
				"%s(): %i / SSL_ERROR_SSL\n",
				func, val);
			break;
	}
	do {
		errid = ERR_get_error();
		if (errid) {
			errmsg = ERR_error_string(errid, NULL);
			fprintf(stderr, "\t%s\n", errmsg);
		}
	} while (errid != 0);
}

/*static const char *
ssl_get_x509_err (long verify_res)
{
	sprintf(_ssl_error_code, "%ld", verify_res);
	return _ssl_error_code;
}*/

	
int
ssl_verify_cb (int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	/* As this callback doesn't get auxiliary pointer parameter we
	 * cannot really use this. However, we can retrieve results later. */
	return 1;
}

static gboolean
ssl_verify_certificate (LmSSL *ssl, const gchar *server)
{
	gboolean retval = TRUE;
	LmSSLBase *base;
	long verify_res;
	unsigned int digest_len;
	X509 *srv_crt;
	gchar *cn;
	X509_NAME *crt_subj;

	base = LM_SSL_BASE(ssl);

	fprintf(stderr, "%s: Cipher: %s/%s/%i\n",
		__FILE__,
		SSL_get_cipher_version(ssl->ssl),
		SSL_get_cipher_name(ssl->ssl),
		SSL_get_cipher_bits(ssl->ssl, NULL));
	verify_res = SSL_get_verify_result(ssl->ssl);
	srv_crt = SSL_get_peer_certificate(ssl->ssl);
	if (base->expected_fingerprint != NULL) {
		X509_digest(srv_crt, EVP_md5(), (guchar *) base->fingerprint,
			&digest_len);
		if (memcmp(base->expected_fingerprint, base->fingerprint,
			digest_len) != 0) {
			if (base->func(ssl,
				LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				return FALSE;
			}
		}
	}
	fprintf(stderr, "%s: SSL_get_verify_result() = %ld\n",
		__FILE__,
		verify_res);
	switch (verify_res) {
		case X509_V_OK:
			break;
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			/* special case for self signed certificates? */
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		case X509_V_ERR_UNABLE_TO_GET_CRL:
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
			if (base->func(ssl,
				LM_SSL_STATUS_NO_CERT_FOUND,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				retval = FALSE;
			}
			break;
		case X509_V_ERR_INVALID_CA:
		case X509_V_ERR_CERT_UNTRUSTED:
		case X509_V_ERR_CERT_REVOKED:
			if (base->func(ssl,
				LM_SSL_STATUS_UNTRUSTED_CERT,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				retval = FALSE;
			}
			break;
		case X509_V_ERR_CERT_NOT_YET_VALID:
		case X509_V_ERR_CRL_NOT_YET_VALID:
			if (base->func(ssl,
				LM_SSL_STATUS_CERT_NOT_ACTIVATED,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				retval = FALSE;
			}
			break;
		case X509_V_ERR_CERT_HAS_EXPIRED:
		case X509_V_ERR_CRL_HAS_EXPIRED:
			if (base->func(ssl,
				LM_SSL_STATUS_CERT_EXPIRED,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				retval = FALSE;
			}
			break;
		default:
			if (base->func(ssl, LM_SSL_STATUS_GENERIC_ERROR,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				retval = FALSE;
			}
	}
	/*if (retval == FALSE) {
		g_set_error (error, LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			ssl_get_x509_err(verify_res), NULL);
	}*/
	crt_subj = X509_get_subject_name(srv_crt);
	cn = (gchar *) g_malloc0(LM_SSL_CN_MAX + 1);
	if (cn == NULL) {
		fprintf(stderr, "g_malloc0() out of memory @ %s:%d\n",
			__FILE__, __LINE__);
		abort();
	}
	if (X509_NAME_get_text_by_NID(crt_subj, NID_commonName, cn,
		LM_SSL_CN_MAX) > 0) {
	fprintf(stderr, "%s: server = '%s', cn = '%s'\n",
		__FILE__, server, cn);
		if (strncmp(server, cn, LM_SSL_CN_MAX) != 0) {
			if (base->func(ssl,
				LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH,
				base->func_data) != LM_SSL_RESPONSE_CONTINUE) {
				retval = FALSE;
			}
		}
	} else {
		fprintf(stderr, "X509_NAME_get_text_by_NID() failed\n");
	}
	fprintf(stderr, "%s:\n\tIssuer: %s\n\tSubject: %s\n\tFor: %s\n",
		__FILE__,
		X509_NAME_oneline(X509_get_issuer_name(srv_crt), NULL, 0),
		X509_NAME_oneline(X509_get_subject_name(srv_crt), NULL, 0),
		cn);
	g_free(cn);
	
	return retval;
}

static GIOStatus
ssl_io_status_from_return (LmSSL *ssl, gint ret)
{
	gint      error;
	GIOStatus status;

	if (ret > 0) return G_IO_STATUS_NORMAL;

	error = SSL_get_error(ssl->ssl, ret);
	switch (error) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			status = G_IO_STATUS_AGAIN;
			break;
		case SSL_ERROR_ZERO_RETURN:
			status = G_IO_STATUS_EOF;
			break;
		default:
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
	static gboolean initialized = FALSE;
	/*const char *cert_file = NULL;*/

	if (!initialized) {
		SSL_library_init();
		/* FIXME: Is this needed when we are not in debug? */
		SSL_load_error_strings();
		initialized = TRUE;
	}

	ssl->ssl_method = TLSv1_client_method();
	if (ssl->ssl_method == NULL) {
		fprintf(stderr, "TLSv1_client_method() == NULL\n");
		abort();
	}
	ssl->ssl_ctx = SSL_CTX_new(ssl->ssl_method);
	if (ssl->ssl_ctx == NULL) {
		fprintf(stderr, "SSL_CTX_new() == NULL\n");
		abort();
	}
	/*if (access("/etc/ssl/cert.pem", R_OK) == 0)
		cert_file = "/etc/ssl/cert.pem";
	if (!SSL_CTX_load_verify_locations(ssl->ssl_ctx,
		cert_file, "/etc/ssl/certs")) {
		fprintf(stderr, "SSL_CTX_load_verify_locations() failed\n");
	}*/
	SSL_CTX_set_default_verify_paths(ssl->ssl_ctx);
	SSL_CTX_set_verify(ssl->ssl_ctx, SSL_VERIFY_PEER, ssl_verify_cb);
}

gboolean
_lm_ssl_begin (LmSSL *ssl, gint fd, const gchar *server, GError **error)
{
	gint ssl_ret;
	GIOStatus status;

	ssl->ssl = SSL_new(ssl->ssl_ctx);
	if (ssl->ssl == NULL) {
		fprintf(stderr, "SSL_new() == NULL\n");
		g_set_error(error, LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			"SSL_new()");
		return FALSE;
	}
	if (!SSL_set_fd(ssl->ssl, fd)) {
		fprintf(stderr, "SSL_set_fd() failed\n");
		g_set_error(error, LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			"SSL_set_fd()");
		return FALSE;
	}
	/*ssl->bio = BIO_new_socket (fd, BIO_NOCLOSE);
	if (ssl->bio == NULL) {
		fprintf(stderr, "BIO_new_socket() failed\n");
		g_set_error(error, LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			"BIO_new_socket()");
		return FALSE;
	}
	SSL_set_bio(ssl->ssl, ssl->bio, ssl->bio);*/

	do {
		ssl_ret = SSL_connect(ssl->ssl);
		if (ssl_ret <= 0) {
			status = ssl_io_status_from_return(ssl, ssl_ret);
			if (status != G_IO_STATUS_AGAIN) {
				ssl_print_state(ssl, "SSL_connect",
					ssl_ret);
				g_set_error(error, LM_ERROR,
					LM_ERROR_CONNECTION_OPEN,
					"SSL_connect()");
				return FALSE;
			}

		}
	} while (ssl_ret <= 0);

	if (!ssl_verify_certificate (ssl, server)) {
		g_set_error (error, LM_ERROR, LM_ERROR_CONNECTION_OPEN,
			"*** SSL certificate verification failed");
		return FALSE;
	}
	
	return TRUE; 
}

GIOStatus
_lm_ssl_read (LmSSL *ssl, gchar *buf, gint len, gsize *bytes_read)
{
	GIOStatus status;
	gint ssl_ret;

	*bytes_read = 0;
	ssl_ret = SSL_read(ssl->ssl, buf, len);
	status = ssl_io_status_from_return(ssl, ssl_ret);
	if (status == G_IO_STATUS_NORMAL) {
		*bytes_read = ssl_ret;
	}
	
	return status;
}

gint
_lm_ssl_send (LmSSL *ssl, const gchar *str, gint len)
{
	GIOStatus status;
	gint ssl_ret;

	do {
		ssl_ret = SSL_write(ssl->ssl, str, len);
		if (ssl_ret <= 0) {
			status = ssl_io_status_from_return(ssl, ssl_ret);
			if (status != G_IO_STATUS_AGAIN)
				return -1;
		}
	} while (ssl_ret <= 0);

	return ssl_ret;
}

void 
_lm_ssl_close (LmSSL *ssl)
{
	SSL_shutdown(ssl->ssl);
	SSL_free(ssl->ssl);
	ssl->ssl = NULL;
}

void
_lm_ssl_free (LmSSL *ssl)
{
	SSL_CTX_free(ssl->ssl_ctx);
	ssl->ssl_ctx = NULL;

	_lm_ssl_base_free_fields (LM_SSL_BASE(ssl));
	g_free (ssl);
}

#endif /* HAVE_GNUTLS */
