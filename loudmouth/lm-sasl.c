/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "lm-sock.h"
#include "lm-debug.h"
#include "lm-error.h"
#include "lm-internals.h"
#include "lm-message-queue.h"
#include "lm-misc.h"
#include "lm-ssl-internals.h"
#include "lm-parser.h"
#include "lm-sha.h"
#include "lm-connection.h"
#include "lm-utils.h"
#include "lm-socket.h"
#include "lm-sasl.h"

#include "md5.h"
#include "base64.h"

typedef enum {
	AUTH_TYPE_PLAIN  = 1,
	AUTH_TYPE_DIGEST = 2
} AuthType;

typedef enum {
	SASL_AUTH_STATE_NO_MECH,
	SASL_AUTH_STATE_PLAIN_STARTED,
	SASL_AUTH_STATE_DIGEST_MD5_STARTED,
	SASL_AUTH_STATE_DIGEST_MD5_SENT_AUTH_RESPONSE,
	SASL_AUTH_STATE_DIGEST_MD5_SENT_FINAL_RESPONSE,
} SaslAuthState;

struct _LmSASL {
	LmConnection        *connection;
	AuthType             auth_type;
	SaslAuthState        state;
	gchar               *username;
	gchar               *password;
	gchar               *server;
	gchar               *digest_md5_rspauth;
	LmMessageHandler    *features_cb;
	LmMessageHandler    *challenge_cb;
	LmMessageHandler    *success_cb;
	LmMessageHandler    *failure_cb;

	gboolean             features_received;
	gboolean             start_auth;

	LmSASLResultHandler  handler;
};

#define XMPP_NS_SASL_AUTH "urn:ietf:params:xml:ns:xmpp-sasl"

static LmHandlerResult     sasl_features_cb  (LmMessageHandler *handler,
					      LmConnection     *connection,
					      LmMessage        *message,
					      gpointer          user_data);

static LmHandlerResult     sasl_challenge_cb (LmMessageHandler *handler,
					      LmConnection     *connection,
					      LmMessage        *message,
					      gpointer          user_data);

static LmHandlerResult     sasl_success_cb   (LmMessageHandler *handler,
					      LmConnection     *connection,
					      LmMessage        *message,
					      gpointer          user_data);

static LmHandlerResult     sasl_failure_cb   (LmMessageHandler *handler,
					      LmConnection     *connection,
					      LmMessage        *message,
					      gpointer          user_data);


/* DIGEST-MD5 mechanism code from libgibber */

static gchar *
sasl_strndup_unescaped (const gchar *str, gsize len) 
{
	const gchar *s;
	gchar       *d;
	gchar       *ret;

	ret = g_malloc0 (len + 1);
	for (s = str, d = ret ; s < (str + len) ; s++, d++) {
		if (*s == '\\') s++;
		*d = *s;
	}

	return ret;
}

static GHashTable *
sasl_digest_md5_challenge_to_hash (const gchar * challenge)
{
	const gchar *keystart, *keyend, *valstart;
	const gchar *c = challenge;
	gchar       *key, *val;
	GHashTable  *result;
	
	result = g_hash_table_new_full (g_str_hash, g_str_equal, 
					g_free, g_free);

	do { 
		while (g_ascii_isspace(*c)) c++;

		keystart = c;
		for (; *c != '\0' && *c != '='; c++);

		if (*c == '\0' || c == keystart) goto error;

		keyend = c; 
		c++;

		if (*c == '"') {
			c++;
			valstart = c;
			for (; *c != '\0' && *c != '"'; c++);
			if (*c == '\0' || c == valstart) goto error;
			val = sasl_strndup_unescaped (valstart, c - valstart);
			c++;
		} else {
			valstart = c;
			for (; *c !=  '\0' && *c != ','; c++);
			if (c == valstart) goto error;
			val = g_strndup (valstart, c - valstart);
		}

		key = g_strndup (keystart, keyend - keystart);

		g_hash_table_insert (result, key, val);

		if (*c == ',') c++;
	} while (*c != '\0');

	return result;
error:
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL, 
	       "Failed to parse challenge: %s", challenge);

	g_hash_table_destroy (result);
	return NULL;
}

static gchar *
sasl_md5_hex_hash (gchar *value, gsize len) 
{
	md5_byte_t   digest_md5[16];
	md5_state_t  md5_calc;
	GString     *str;
	int          i;

	str = g_string_sized_new (32);

	md5_init (&md5_calc);
	md5_append (&md5_calc, (const md5_byte_t *)value, len);
	md5_finish (&md5_calc, digest_md5);

	for (i = 0 ; i < 16 ; i++) {
		g_string_append_printf (str, "%02x", digest_md5[i]);
	}

	return g_string_free (str, FALSE);
}

static gchar *
sasl_digest_md5_generate_cnonce(void)
{
	/* RFC 2831 recommends the the nonce to be either hexadecimal or base64 with
	 * at least 64 bits of entropy */
#define NR 8
	guint32 n[NR]; 
	int i;

	for (i = 0; i < NR; i++) {
		n[i] = g_random_int();
	}

	return base64_encode ((gchar *)n, sizeof(n));
}

static gchar *
sasl_md5_prepare_response (LmSASL *sasl, GHashTable *challenge)
{
	GString     *response;
	const gchar *realm, *nonce;
	gchar       *a1, *a1h, *a2, *a2h, *kd, *kdh;
	gchar       *cnonce = NULL;
	gchar       *tmp;
	md5_byte_t   digest_md5[16];
	md5_state_t  md5_calc;
	gsize        len;

	response = g_string_new ("");

	if (sasl->username == NULL || sasl->password == NULL) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		       "%s: no username or password provided", G_STRFUNC);
		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection, 
				       FALSE, "no username/password provided");
		}
		goto error;
	}

	nonce = g_hash_table_lookup (challenge, "nonce");
	if (nonce == NULL || nonce == '\0') {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		       "%s: server didn't provide a nonce in the challenge", 
		       G_STRFUNC);
		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection,
				       FALSE, "server error");
		}
		goto error;
	}

	cnonce = sasl_digest_md5_generate_cnonce ();

	/* FIXME challenge can contain multiple realms */
	realm = g_hash_table_lookup (challenge, "realm");
	if (realm == NULL) {
		realm = sasl->server;
	}

	/* FIXME properly escape values */
	g_string_append_printf (response, "username=\"%s\"", sasl->username);
	g_string_append_printf (response, ",realm=\"%s\"", realm);
	g_string_append_printf (response, ",digest-uri=\"xmpp/%s\"", realm);
	g_string_append_printf (response, ",nonce=\"%s\",nc=00000001", nonce);
	g_string_append_printf (response, ",cnonce=\"%s\"", cnonce);
	/* FIXME should check if auth is in the cop challenge val */
	g_string_append_printf (response, ",qop=auth,charset=utf-8");

	tmp = g_strdup_printf ("%s:%s:%s", 
			       sasl->username, realm, sasl->password);
	md5_init (&md5_calc);
	md5_append (&md5_calc, (const md5_byte_t *)tmp, strlen(tmp));
	md5_finish (&md5_calc, digest_md5);
	g_free (tmp);

	a1 = g_strdup_printf ("0123456789012345:%s:%s", nonce, cnonce);
	len = strlen (a1);
	memcpy (a1, digest_md5, 16);
	a1h = sasl_md5_hex_hash (a1, len);

	a2 = g_strdup_printf ("AUTHENTICATE:xmpp/%s", realm);
	a2h = sasl_md5_hex_hash (a2, strlen(a2));

	kd = g_strdup_printf ("%s:%s:00000001:%s:auth:%s",
			      a1h, nonce, cnonce, a2h);
	kdh = sasl_md5_hex_hash (kd, strlen(kd));
	g_string_append_printf (response, ",response=%s", kdh);

	g_free (kd);
	g_free (kdh);
	g_free (a2);
	g_free (a2h);

	/* Calculate the response we expect from the server */
	a2 = g_strdup_printf (":xmpp/%s", realm);
	a2h = sasl_md5_hex_hash (a2, strlen(a2));

	kd = g_strdup_printf ("%s:%s:00000001:%s:auth:%s", a1h, nonce, cnonce, a2h);
	g_free (sasl->digest_md5_rspauth);
	sasl->digest_md5_rspauth = sasl_md5_hex_hash (kd, strlen(kd));

	g_free (a1);
	g_free (a1h);
	g_free (a2);
	g_free (a2h);
	g_free (kd);

out:
	g_free (cnonce);
	if (response) {
		return g_string_free (response, FALSE);
	} else {
		return NULL;
	}

error:
	g_string_free (response, TRUE);
	response = NULL;
	goto out;
}

static gboolean
sasl_digest_md5_send_initial_response (LmSASL *sasl, GHashTable *challenge)
{
	LmMessage *msg;
	gchar     *response;
	gchar     *response64;
	int        result;

	response = sasl_md5_prepare_response(sasl, challenge);
	if (response == NULL) {
		return FALSE;
	}

	response64 = base64_encode ((gchar *)response, strlen(response));

	msg = lm_message_new (NULL, LM_MESSAGE_TYPE_RESPONSE);
	lm_message_node_set_attributes (msg->node,
					"xmlns", XMPP_NS_SASL_AUTH,
					NULL);
	lm_message_node_set_value (msg->node, response64);

	result = lm_connection_send (sasl->connection, msg, NULL);

	g_free (response);
	g_free (response64);
	lm_message_unref (msg);

	if (!result) {
		return FALSE;
	}

	sasl->state = SASL_AUTH_STATE_DIGEST_MD5_SENT_AUTH_RESPONSE;

	return TRUE;
}

static gboolean
sasl_digest_md5_check_server_response(LmSASL *sasl, GHashTable *challenge)
{
	LmMessage   *msg;
	const gchar *rspauth;
	int          result;

	rspauth = g_hash_table_lookup (challenge, "rspauth");
	if (rspauth == NULL) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SSL,
		       "%s: server sent an invalid reply (no rspauth)\n",
		       G_STRFUNC);

		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection, 
				       TRUE, "server error");
		}
		return FALSE;
	}

	if (strcmp (sasl->digest_md5_rspauth, rspauth) != 0) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SSL,
		       "%s: server sent an invalid reply (rspauth not matching)\n", 
		       G_STRFUNC);

		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection,
				       TRUE, "server error");
		}
		return FALSE;
	}

	msg = lm_message_new (NULL, LM_MESSAGE_TYPE_RESPONSE);
	lm_message_node_set_attributes (msg->node,
					"xmlns", XMPP_NS_SASL_AUTH,
					NULL);

	result = lm_connection_send (sasl->connection, msg, NULL);
	lm_message_unref (msg);

	if (!result) {
		g_warning ("Failed to send SASL response\n");
		return FALSE;
	}

	sasl->state = SASL_AUTH_STATE_DIGEST_MD5_SENT_FINAL_RESPONSE;

	return TRUE;
}

static gboolean
sasl_digest_md5_handle_challenge (LmSASL *sasl, LmMessageNode *node)
{
	const gchar *encoded;
	gchar       *challenge;
	gsize        len;
	GHashTable  *h;

	encoded = lm_message_node_get_value (node);
	if (!encoded) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		       "%s: got empty challenge!", G_STRFUNC);
		return FALSE;
	}

	challenge = (gchar *) base64_decode (encoded, &len);
	h = sasl_digest_md5_challenge_to_hash (challenge);
	g_free(challenge);

	if (!h) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		       "%s: server sent an invalid challenge", G_STRFUNC);
		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection, 
				       FALSE, "server error");
		}
		return FALSE;
	}

	switch (sasl->state) {
	case SASL_AUTH_STATE_DIGEST_MD5_STARTED:
		sasl_digest_md5_send_initial_response (sasl, h); 
		break;
	case SASL_AUTH_STATE_DIGEST_MD5_SENT_AUTH_RESPONSE:
		sasl_digest_md5_check_server_response (sasl, h); 
		break;
	default:
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		       "%s: server sent a challenge at the wrong time", 
		       G_STRFUNC);
		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection,
				       FALSE, "server error");
		}

		return FALSE;
	} 

	g_hash_table_destroy(h);

	return TRUE;
}

static LmHandlerResult
sasl_challenge_cb (LmMessageHandler *handler,
		   LmConnection     *connection,
		   LmMessage        *message,
		   gpointer          user_data)
{
	LmSASL      *sasl;
	const gchar *ns;

	ns = lm_message_node_get_attribute (message->node, "xmlns");
	if (!ns || strcmp (ns, XMPP_NS_SASL_AUTH) != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	sasl = (LmSASL *) user_data;

	switch (sasl->auth_type) {
	case AUTH_TYPE_PLAIN:
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SSL,
		       "%s: server sent challenge for PLAIN mechanism",
		       G_STRFUNC);

		if (sasl->handler) {
			sasl->handler (sasl, sasl->connection, 
				       FALSE, "server error");
		}
		break;
	case AUTH_TYPE_DIGEST:
		sasl_digest_md5_handle_challenge (sasl, message->node);
		break;
	default:
		g_warning ("Wrong auth type");
		break;
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
sasl_success_cb (LmMessageHandler *handler,
		 LmConnection     *connection,
		 LmMessage        *message,
		 gpointer          user_data)
{
	LmSASL      *sasl;
	const gchar *ns;
	
	ns = lm_message_node_get_attribute (message->node, "xmlns");
	if (!ns || strcmp (ns, XMPP_NS_SASL_AUTH) != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	sasl = (LmSASL *) user_data;

	switch (sasl->auth_type) {
	case AUTH_TYPE_PLAIN:
		if (sasl->state != SASL_AUTH_STATE_PLAIN_STARTED) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
			       "%s: server sent success before finishing auth", 
			       G_STRFUNC);
			if (sasl->handler) {
				sasl->handler (sasl, sasl->connection, 
					       FALSE, "server error");
			}
		}
		break;
	case AUTH_TYPE_DIGEST:
		if (sasl->state != SASL_AUTH_STATE_DIGEST_MD5_SENT_AUTH_RESPONSE &&
		    sasl->state != SASL_AUTH_STATE_DIGEST_MD5_SENT_FINAL_RESPONSE) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
			       "%s: server sent success before finishing auth", 
			       G_STRFUNC);
			if (sasl->handler) {
				sasl->handler (sasl, sasl->connection, 
					       FALSE, "server error");
			}
		}
		break;
	default:
		g_warning ("Wrong auth type");
		break;
	}

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
	       "%s: SASL authentication successful", G_STRFUNC);

	if (sasl->handler) {
		sasl->handler (sasl, sasl->connection, TRUE, NULL);
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	
}

static LmHandlerResult
sasl_failure_cb (LmMessageHandler *handler,
		 LmConnection     *connection,
		 LmMessage        *message,
		 gpointer          user_data)
{
	LmSASL      *sasl;
	const gchar *ns;
	const gchar *reason = "unknown reason";
	
	ns = lm_message_node_get_attribute (message->node, "xmlns");
	if (!ns || strcmp (ns, XMPP_NS_SASL_AUTH) != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	sasl = (LmSASL *) user_data;

	if (message->node->children) {
		const gchar *r;
		
		r = lm_message_node_get_value (message->node->children);
		if (r) {
			reason = r;
		}
	}
	
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
	       "%s: SASL authentication failed: %s", G_STRFUNC, reason);

	if (sasl->handler) {
		sasl->handler (sasl, sasl->connection, FALSE, reason);
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}


static gboolean
sasl_start (LmSASL *sasl)
{
	LmMessage  *auth_msg;
	gboolean    result;
	const char *mech = NULL;

	auth_msg = lm_message_new (NULL, LM_MESSAGE_TYPE_AUTH);

	if (sasl->auth_type == AUTH_TYPE_PLAIN) {
      		GString *str;
		gchar   *cstr;

		str = g_string_new ("");

		mech = "PLAIN";
		sasl->state = SASL_AUTH_STATE_PLAIN_STARTED;

		if (sasl->username == NULL || sasl->password == NULL) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
			       "%s: no username or password provided", 
			       G_STRFUNC);
			if (sasl->handler) {
				sasl->handler (sasl, sasl->connection, FALSE, "no username/password provided");
			}

			return FALSE;
		}

		g_string_append_c (str, '\0');
		g_string_append (str, sasl->username);
		g_string_append_c (str, '\0');
		g_string_append (str, sasl->password);
		cstr = base64_encode ((gchar *)str->str, str->len);

		lm_message_node_set_value (auth_msg->node, cstr);

		g_string_free (str, TRUE);
		g_free (cstr);

		/* Here we say the Google magic word. Bad Google. */
		lm_message_node_set_attributes (auth_msg->node,
						"xmlns:ga", "http://www.google.com/talk/protocol/auth",
						"ga:client-uses-full-bind-result", "true",
						NULL);

	} 
	else if (sasl->auth_type == AUTH_TYPE_DIGEST) {
		mech = "DIGEST-MD5";
		sasl->state = SASL_AUTH_STATE_DIGEST_MD5_STARTED;
	}

	lm_message_node_set_attributes (auth_msg->node,
					"xmlns", XMPP_NS_SASL_AUTH,
					"mechanism", mech,
					NULL);

	result = lm_connection_send (sasl->connection, auth_msg, NULL);
	lm_message_unref (auth_msg);

	if (!result) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
sasl_set_auth_type (LmSASL *sasl, LmMessageNode *mechanisms)
{
	LmMessageNode *m;
	const gchar   *ns;

	sasl->auth_type = 0;

	ns = lm_message_node_get_attribute (mechanisms, "xmlns");
	if (!ns || strcmp (ns, XMPP_NS_SASL_AUTH) != 0) {
		return FALSE;
	}

	for (m = mechanisms->children; m; m = m->next) {
		const gchar *name;
		
		name = lm_message_node_get_value (m);

		if (!name) {
			continue;
		}
		if (strcmp (name, "PLAIN") == 0) {
			sasl->auth_type |= AUTH_TYPE_PLAIN;
			continue;
		}
		if (strcmp (name, "DIGEST-MD5") == 0) {
			sasl->auth_type |= AUTH_TYPE_DIGEST;
			continue;
		}

		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		       "%s: unknown SASL auth mechanism: %s", G_STRFUNC, name);
	}

	return TRUE;
}

static gboolean
sasl_authenticate (LmSASL *sasl)
{
	if (sasl->auth_type == 0) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL,
		      "%s: no supported SASL auth mechanisms found",
		      G_STRFUNC);

		return FALSE;
	}

	/* Prefer DIGEST */
	if (sasl->auth_type & AUTH_TYPE_DIGEST) {
		sasl->auth_type = AUTH_TYPE_DIGEST;
		return sasl_start (sasl);
	}
	else if (sasl->auth_type & AUTH_TYPE_PLAIN) {
		sasl->auth_type = AUTH_TYPE_PLAIN;
		return sasl_start (sasl);
	} 

	return FALSE;
}

static LmHandlerResult
sasl_features_cb (LmMessageHandler *handler,
		  LmConnection     *connection,
		  LmMessage        *message,
		  gpointer          user_data)
{
    	LmMessageNode *mechanisms;
	LmSASL        *sasl;

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_SASL, "Stream features received\n");
	mechanisms = lm_message_node_find_child (message->node, "mechanisms");
	if (!mechanisms) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	sasl = (LmSASL *) user_data;
	sasl->features_received = TRUE;

	sasl_set_auth_type (sasl, mechanisms);

	if (sasl->start_auth) {
		sasl_authenticate (sasl);
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

LmSASL *
lm_sasl_new (LmConnection *connection)
{
	LmSASL *sasl;
	
	sasl = g_new0 (LmSASL, 1);

	sasl->connection = connection;
	sasl->features_received = FALSE;
	sasl->start_auth = FALSE;

	sasl->features_cb = lm_message_handler_new (sasl_features_cb,
						    sasl,
						    NULL);

	lm_connection_register_message_handler (connection,
						sasl->features_cb,
						LM_MESSAGE_TYPE_STREAM_FEATURES,
						LM_HANDLER_PRIORITY_LAST);
	return sasl;
}

void
lm_sasl_authenticate (LmSASL              *sasl,
		      const gchar         *username,
		      const gchar         *password,
		      const gchar         *server,
		      LmSASLResultHandler  handler)
{
	sasl->username   = g_strdup (username);
	sasl->password   = g_strdup (password);
	sasl->server     = g_strdup (server);
	sasl->handler    = handler;

	sasl->challenge_cb = lm_message_handler_new (sasl_challenge_cb,
						     sasl,
						     NULL);
	lm_connection_register_message_handler (sasl->connection,
						sasl->challenge_cb,
						LM_MESSAGE_TYPE_CHALLENGE,
						LM_HANDLER_PRIORITY_FIRST);
	
	sasl->success_cb = lm_message_handler_new (sasl_success_cb,
						   sasl,
						   NULL);
	lm_connection_register_message_handler (sasl->connection,
						sasl->success_cb,
						LM_MESSAGE_TYPE_SUCCESS,
						LM_HANDLER_PRIORITY_FIRST);

	sasl->failure_cb = lm_message_handler_new (sasl_failure_cb,
						   sasl,
						   NULL);
	lm_connection_register_message_handler (sasl->connection,
						sasl->failure_cb,
						LM_MESSAGE_TYPE_FAILURE,
						LM_HANDLER_PRIORITY_FIRST);

	if (sasl->features_received) {
		sasl_authenticate (sasl);
	} else {
		sasl->start_auth = TRUE;
	}
}

void
lm_sasl_free (LmSASL *sasl)
{
	g_return_if_fail (sasl != NULL);

	g_free (sasl->username);
	g_free (sasl->password);
	g_free (sasl->server);

	if (sasl->features_cb) {
		lm_connection_unregister_message_handler (sasl->connection,
							  sasl->features_cb, 
							  LM_MESSAGE_TYPE_STREAM_FEATURES);
	}

	if (sasl->challenge_cb) {
		lm_connection_unregister_message_handler (sasl->connection,
							  sasl->challenge_cb,
							  LM_MESSAGE_TYPE_CHALLENGE);
	}

	if (sasl->success_cb) {
		lm_connection_unregister_message_handler (sasl->connection,
							  sasl->success_cb,
							  LM_MESSAGE_TYPE_SUCCESS);
	}

	if (sasl->failure_cb) {
		lm_connection_unregister_message_handler (sasl->connection,
							  sasl->failure_cb,
							  LM_MESSAGE_TYPE_FAILURE);
	}

	g_free (sasl);
}


void
lm_sasl_get_auth_params (LmSASL *sasl, const gchar **username,
	const gchar **password)
{
	*username = sasl->username;
	*password = sasl->password;
}

