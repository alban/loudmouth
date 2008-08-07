/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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
#include <asyncns.h>
#define freeaddrinfo(x) asyncns_freeaddrinfo(x)

/* Needed on Mac OS X */
#if HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif

#include <arpa/nameser.h>
#include <resolv.h>

#include "lm-error.h"
#include "lm-internals.h"
#include "lm-marshal.h"
#include "lm-misc.h"
#include "lm-asyncns-resolver.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_ASYNCNS_RESOLVER, LmAsyncnsResolverPriv))

typedef struct LmAsyncnsResolverPriv LmAsyncnsResolverPriv;
struct LmAsyncnsResolverPriv {
	GSource		*watch_resolv;
	asyncns_query_t *resolv_query;
	asyncns_t	*asyncns_ctx;
	GIOChannel	*resolv_channel;
};

static void     asyncns_resolver_finalize      (GObject     *object);
static void     asyncns_resolver_lookup        (LmResolver  *resolver);
static void     asyncns_resolver_cancel        (LmResolver  *resolver);

G_DEFINE_TYPE (LmAsyncnsResolver, lm_asyncns_resolver, LM_TYPE_RESOLVER)

static void
lm_asyncns_resolver_class_init (LmAsyncnsResolverClass *class)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (class);
        LmResolverClass *resolver_class = LM_RESOLVER_CLASS (class);

	object_class->finalize = asyncns_resolver_finalize;

        resolver_class->lookup = asyncns_resolver_lookup;
        resolver_class->cancel = asyncns_resolver_cancel;

	g_type_class_add_private (object_class, sizeof (LmAsyncnsResolverPriv));
}

static void
lm_asyncns_resolver_init (LmAsyncnsResolver *asyncns_resolver)
{
	LmAsyncnsResolverPriv *priv;

	priv = GET_PRIV (asyncns_resolver);
}

static void
asyncns_resolver_finalize (GObject *object)
{
	LmAsyncnsResolverPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (lm_asyncns_resolver_parent_class)->finalize) (object);
}

static void
asyncns_resolver_cleanup (LmResolver *resolver)
{
        LmAsyncnsResolverPriv *priv = GET_PRIV (resolver);

        if (priv->resolv_channel != NULL) {
		g_io_channel_unref (priv->resolv_channel);
                priv->resolv_channel = NULL;
	}
 
        if (priv->watch_resolv) {
		g_source_destroy (priv->watch_resolv);
                priv->watch_resolv = NULL;
	}

	if (priv->asyncns_ctx) {
                asyncns_free (priv->asyncns_ctx);
                priv->asyncns_ctx = NULL;
	}

        priv->resolv_query = NULL;
}

static void
asyncns_resolver_done (LmResolver *resolver)
{
        LmAsyncnsResolverPriv *priv = GET_PRIV (resolver);
        struct addrinfo	      *ans;
	int 		       err;

        err = asyncns_getaddrinfo_done (priv->asyncns_ctx, priv->resolv_query, &ans);
        priv->resolv_query = NULL;
        /* Signal that we are done */

        g_object_ref (resolver);

        if (err) {
                _lm_resolver_set_result (resolver,
                                         LM_RESOLVER_RESULT_FAILED, 
                                         NULL);
        } else {
                _lm_resolver_set_result (resolver,
                                         LM_RESOLVER_RESULT_OK,
                                         ans);
        }

        asyncns_resolver_cleanup (resolver);

        g_object_unref (resolver);
}

typedef gboolean  (* LmAsyncnsResolverCallback) (LmResolver *resolver);

static gboolean
asyncns_resolver_io_cb (GSource      *source,
                        GIOCondition  condition,
                        LmResolver   *resolver)
{
        LmAsyncnsResolverPriv     *priv = GET_PRIV (resolver);
        LmAsyncnsResolverCallback  func;

        asyncns_wait (priv->asyncns_ctx, FALSE);

        if (!asyncns_isdone (priv->asyncns_ctx, priv->resolv_query)) {
                return TRUE;
        }

        func = (LmAsyncnsResolverCallback) asyncns_getuserdata (priv->asyncns_ctx,
                                                                priv->resolv_query);
        return func (resolver);
}

static gboolean
asyncns_resolver_prep (LmResolver *resolver, GError **error)
{
        LmAsyncnsResolverPriv *priv = GET_PRIV (resolver);
        GMainContext          *context;

        if (priv->asyncns_ctx) {
                return TRUE;
	}

        priv->asyncns_ctx = asyncns_new (1);
	if (priv->asyncns_ctx == NULL) {
                g_set_error (error,
                             LM_ERROR,                 
                             LM_ERROR_CONNECTION_FAILED,   
                             "can't initialise libasyncns");
		return FALSE;
	}

        priv->resolv_channel =
                g_io_channel_unix_new (asyncns_fd (priv->asyncns_ctx));

        g_object_get (resolver, "context", &context, NULL);
        
        g_print ("Setting up io watch\n");

        priv->watch_resolv = 
                lm_misc_add_io_watch (context,
                                      priv->resolv_channel,
				      G_IO_IN,
                                      (GIOFunc) asyncns_resolver_io_cb,
                                      resolver);

	return TRUE;
}

static void
asyncns_resolver_lookup_host (LmResolver *resolver)
{
        LmAsyncnsResolverPriv *priv = GET_PRIV (resolver);
        gchar               *host;
        struct addrinfo      req;

        g_object_get (resolver, "host", &host, NULL);

	memset (&req, 0, sizeof(req));
	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;

	if (!asyncns_resolver_prep (resolver, NULL)) {
                g_warning ("Signal error\n");
		return;
        }

        priv->resolv_query =
	  	asyncns_getaddrinfo (priv->asyncns_ctx,
                                     host,
				     NULL,
				     &req);

	asyncns_setuserdata (priv->asyncns_ctx,
                             priv->resolv_query,
			     (gpointer) asyncns_resolver_done);
}

static void
asyncns_resolver_srv_done (LmResolver *resolver)
{
        LmAsyncnsResolverPriv *priv = GET_PRIV (resolver);
        unsigned char         *srv_ans;
	int 		       srv_len;
        gboolean               result = FALSE;

        g_print ("srv_done callback\n");

        srv_len = asyncns_res_done (priv->asyncns_ctx, 
                                    priv->resolv_query, &srv_ans);

        priv->resolv_query = NULL;

        if (srv_len <= 0) {
                /* FIXME: Report error */
                g_warning ("Failed to read srv request results");
        } else {
                gchar *new_server;
                guint  new_port;

                g_print ("trying to parse srv response\n");

                result = _lm_resolver_parse_srv_response (srv_ans, srv_len,
                                                          &new_server,
                                                          &new_port);
                if (result == TRUE) {
                        g_print ("worked, new host/post is %s/%d\n",
                                 new_server, new_port);

                        g_object_set (resolver,
                                      "host", new_server,
                                      "port", new_port,
                                      NULL);
                }

                g_free (new_server);
                /* TODO: Check whether srv_ans needs freeing */
        }

        asyncns_resolver_cleanup (resolver);

        if (result == TRUE) {
                asyncns_resolver_lookup_host (resolver);
        }
}

static void
asyncns_resolver_lookup_service (LmResolver *resolver)
{
        LmAsyncnsResolverPriv *priv = GET_PRIV (resolver);
        gchar                 *domain;
        gchar                 *service;
        gchar                 *protocol;
        gchar                 *srv;


        g_object_get (resolver,
                      "domain", &domain,
                      "service", &service,
                      "protocol", &protocol,
                      NULL);
        
        srv = _lm_resolver_create_srv_string (domain, service, protocol);
        
        g_print ("Looking up service: %s %s %s\n[%s]", domain, service, protocol, srv);

        if (!asyncns_resolver_prep (resolver, /* Use GError? */ NULL)) {
                g_warning ("Failed to initiate the asyncns library");
                /* FIXME: Signal error */
                return;
        }

        priv->resolv_query =
                asyncns_res_query (priv->asyncns_ctx, srv, C_IN, T_SRV);
        
        asyncns_setuserdata (priv->asyncns_ctx, 
                             priv->resolv_query, 
                             (gpointer) asyncns_resolver_srv_done);

        g_free (srv);
        g_free (domain);
        g_free (service);
        g_free (protocol);
}

static void
asyncns_resolver_lookup (LmResolver *resolver)
{
        gint type;

        /* Start the DNS querying */

        /* Decide if we are going to lookup a srv or host */
        g_object_get (resolver, "type", &type, NULL);

        switch (type) {
        case LM_RESOLVER_HOST:
                asyncns_resolver_lookup_host (resolver);
                break;
        case LM_RESOLVER_SRV:
                asyncns_resolver_lookup_service (resolver);
                break;
        };

        /* End of DNS querying */
} 

static void
asyncns_resolver_cancel (LmResolver *resolver)
{
        LmAsyncnsResolverPriv *priv;

        g_return_if_fail (LM_IS_ASYNCNS_RESOLVER (resolver));

        priv = GET_PRIV (resolver);

        if (priv->asyncns_ctx) {
                if (priv->resolv_query) {
                        asyncns_cancel (priv->asyncns_ctx, priv->resolv_query);
                        priv->resolv_query = NULL;
                }

                _lm_resolver_set_result (resolver,
                                         LM_RESOLVER_RESULT_CANCELLED,
                                         NULL);
        }
}

