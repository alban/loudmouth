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

#include <config.h>

#include "lm-ssl.h"
#include "lm-ssl-base.h"
#include "lm-ssl-internals.h"

LmSSLResponse  
_lm_ssl_func_always_continue (LmSSL       *ssl,
			      LmSSLStatus  status,
			      gpointer     user_data)
{
	return LM_SSL_RESPONSE_CONTINUE;;
}

/* Define the SSL functions as noops if we compile without support */
#ifndef HAVE_SSL

LmSSL *
_lm_ssl_new (const gchar    *expected_fingerprint,
	     LmSSLFunction   ssl_function,
	     gpointer        user_data,
	     GDestroyNotify  notify)
{
	return NULL;
}

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

void
_lm_ssl_free (LmSSL *ssl)
{
	/* NOOP */
}

#endif /* HAVE_SSL */



/**
 * lm_ssl_new:
 * @expected_fingerprint: The expected fingerprint. @ssl_function will be called if there is a mismatch. %NULL if you are not interested in this check.
 * @ssl_function: Callback called to inform the user of a problem during setting up the SSL connection and how to proceed. If %NULL is passed the default function that always continues will be used.
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
	/* The implementation of this function will be different depending
	 * on which implementation is used 
	 */
	return _lm_ssl_new (expected_fingerprint,
			    ssl_function, user_data, notify);
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
#ifdef HAVE_SSL
	return TRUE;
#else
	return FALSE;
#endif
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
	
	return LM_SSL_BASE(ssl)->fingerprint;
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

	LM_SSL_BASE(ssl)->ref_count++;

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
	LmSSLBase *base;
	
	g_return_if_fail (ssl != NULL);
        
	base = LM_SSL_BASE (ssl);

	base->ref_count --;
        
        if (base->ref_count == 0) {
		if (base->data_notify) {
			(* base->data_notify) (base->func_data);
		}
             
		_lm_ssl_free (ssl);
        }
}


