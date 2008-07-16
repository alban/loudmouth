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
#include <sys/types.h>
#include <netdb.h>

/* Needed on Mac OS X */
#if HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif

#include <arpa/nameser.h>
#include <resolv.h>

#include "lm-marshal.h"
#include "lm-misc.h"

#include "lm-blocking-resolver.h"

#define SRV_LEN 8192

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_BLOCKING_RESOLVER, LmBlockingResolverPriv))

typedef struct LmBlockingResolverPriv LmBlockingResolverPriv;
struct LmBlockingResolverPriv {
        GSource *idle_source;
};

static void     blocking_resolver_finalize    (GObject       *object);
static void     blocking_resolver_lookup      (LmResolver    *resolver);
static void     blocking_resolver_cancel      (LmResolver    *resolver);

G_DEFINE_TYPE (LmBlockingResolver, lm_blocking_resolver, LM_TYPE_RESOLVER)

static void
lm_blocking_resolver_class_init (LmBlockingResolverClass *class)
{
        GObjectClass    *object_class   = G_OBJECT_CLASS (class);
        LmResolverClass *resolver_class = LM_RESOLVER_CLASS (class);

	object_class->finalize = blocking_resolver_finalize;

        resolver_class->lookup = blocking_resolver_lookup;
        resolver_class->cancel = blocking_resolver_cancel;
	
	g_type_class_add_private (object_class, 
                                  sizeof (LmBlockingResolverPriv));
}

static void
lm_blocking_resolver_init (LmBlockingResolver *blocking_resolver)
{
	LmBlockingResolverPriv *priv;

	priv = GET_PRIV (blocking_resolver);
}

static void
blocking_resolver_finalize (GObject *object)
{
	LmBlockingResolverPriv *priv;

	priv = GET_PRIV (object);

        /* Ensure we don't have an idle around */
        blocking_resolver_cancel (LM_RESOLVER (object));

	(G_OBJECT_CLASS (lm_blocking_resolver_parent_class)->finalize) (object);
}

static void
blocking_resolver_lookup_host (LmBlockingResolver *resolver)
{
        gchar           *host;
        struct addrinfo  req;
        struct addrinfo *ans;
        int              err;

        g_object_get (resolver, "host", &host, NULL);

        /* Lookup */

	memset (&req, 0, sizeof(req));
	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;

        err = getaddrinfo (host, NULL, &req, &ans);

	if (err != 0) {
                /* FIXME: Report error */
                g_print ("ERROR: %d in %s\n", err, G_STRFUNC);
		return;
	}

        if (ans == NULL) {
                /* Couldn't find any results */
                /* FIXME: Report no results  */
                g_print ("No results in %s\n", G_STRFUNC);
        }

        /* FIXME: How to set and iterate the results */
        /*priv->results    = ans;
        priv->cur_result = ans; */

        g_print ("Found result for %s\n", host);

        g_free (host);
}

static gboolean
blocking_resolver_parse_srv_response (unsigned char  *srv, 
                                      int             srv_len, 
                                      gchar         **out_server, 
                                      guint          *out_port)
{
	int                  qdcount;
	int                  ancount;
	int                  len;
	const unsigned char *pos;
	unsigned char       *end;
	HEADER              *head;
	char                 name[256];
	char                 pref_name[256];
	guint                pref_port = 0;
	guint                pref_prio = 9999;

	pref_name[0] = 0;

	pos = srv + sizeof (HEADER);
	end = srv + srv_len;
	head = (HEADER *) srv;

	qdcount = ntohs (head->qdcount);
	ancount = ntohs (head->ancount);

	/* Ignore the questions */
	while (qdcount-- > 0 && (len = dn_expand (srv, end, pos, name, 255)) >= 0) {
		g_assert (len >= 0);
		pos += len + QFIXEDSZ;
	}

	/* Parse the answers */
	while (ancount-- > 0 && (len = dn_expand (srv, end, pos, name, 255)) >= 0) {
		/* Ignore the initial string */
		uint16_t pref, weight, port;

		g_assert (len >= 0);
		pos += len;
		/* Ignore type, ttl, class and dlen */
		pos += 10;
		GETSHORT (pref, pos);
		GETSHORT (weight, pos);
		GETSHORT (port, pos);

		len = dn_expand (srv, end, pos, name, 255);
		if (pref < pref_prio) {
			pref_prio = pref;
			strcpy (pref_name, name);
			pref_port = port;
		}
		pos += len;
	}

	if (pref_name[0]) {
		*out_server = g_strdup (pref_name);
		*out_port = pref_port;
		return TRUE;
	} 
	return FALSE;
}

static void
blocking_resolver_lookup_service (LmBlockingResolver *resolver)
{
        gchar *domain;
        gchar *service;
        gchar *protocol;
        gchar *srv;
        gchar *new_server = NULL;
        guint  new_port = 0;
        gboolean  result;
	unsigned char    srv_ans[SRV_LEN];
	int              len;

        g_object_get (resolver,
                      "domain", &domain,
                      "service", &service,
                      "protocol", &protocol,
                      NULL);

        srv = lm_resolver_create_srv_string (domain, service, protocol);

        res_init ();

        len = res_query (srv, C_IN, T_SRV, srv_ans, SRV_LEN);

        result = blocking_resolver_parse_srv_response (srv_ans, len, 
                                                       &new_server, &new_port);
        if (result == FALSE) {
                g_print ("Error while parsing srv response in %s\n", 
                         G_STRFUNC);
                /* FIXME: Report error */
        }

        g_object_set (resolver, 
                      "host", new_server,
                      "port", new_port,
                      NULL);

        /* Lookup the new server and the new port */
        blocking_resolver_lookup_host (resolver);

        g_free (new_server);
        g_free (srv);
        g_free (domain);
        g_free (service);
        g_free (protocol);
}

static gboolean
blocking_resolver_idle_lookup (LmBlockingResolver *resolver) 
{
        LmBlockingResolverPriv *priv = GET_PRIV (resolver);
        gint                    type;

        /* Start the DNS querying */

        /* Decide if we are going to lookup a srv or host */
        g_object_get (resolver, "type", &type, NULL);

        switch (type) {
        case LM_RESOLVER_HOST:
                blocking_resolver_lookup_host (resolver);
                break;
        case LM_RESOLVER_SRV:
                blocking_resolver_lookup_service (resolver);
                break;
        };

        /* End of DNS querying */
        priv->idle_source = NULL;
        return FALSE;
}

static void
blocking_resolver_lookup (LmResolver *resolver)
{
        LmBlockingResolverPriv *priv;
        GMainContext           *context;

        g_return_if_fail (LM_IS_BLOCKING_RESOLVER (resolver));

        priv = GET_PRIV (resolver);

        g_object_get (resolver, "context", &context, NULL);

        priv->idle_source = lm_misc_add_idle (context, 
                                              (GSourceFunc) blocking_resolver_idle_lookup,
                                              resolver);
}

static void
blocking_resolver_cancel (LmResolver *resolver)
{
        LmBlockingResolverPriv *priv;

        g_return_if_fail (LM_IS_BLOCKING_RESOLVER (resolver));

        priv = GET_PRIV (resolver);

        if (priv->idle_source) {
                g_source_destroy (priv->idle_source);
                priv->idle_source = NULL;
        }
}
