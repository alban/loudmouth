/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB 
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
#include <stdio.h>
#include <time.h>

#include "lm-internals.h"
#include "lm-utils.h"

LmCallback *
_lm_utils_new_callback (gpointer func, 
			gpointer user_data,
			GDestroyNotify notify)
{
	LmCallback *cb;
	
	cb = g_new0 (LmCallback, 1);
	cb->func = func;
	cb->user_data = user_data;
	cb->notify = notify;

	return cb;
}

void
_lm_utils_free_callback (LmCallback *cb)
{
	if (!cb) {
		return;
	}

	if (cb->notify) {
		(* cb->notify) (cb->user_data);
	}
	g_free (cb);
}

gchar *
_lm_utils_generate_id (void)
{
	static gint  number = 0;

	return g_strdup_printf ("msg_%d", ++number);
}

gchar * 
_lm_utils_base64_encode (const gchar *s)
{
	static const gchar *base64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	guint    i, j;
	guint32  bits = 0;
	guint    maxlen = (strlen(s) * 2) + 3;
	gchar   *str;

	str = g_malloc(maxlen);

	j = 0;
	for (i = 0; i < strlen(s); i++) {
		bits <<= 8;
		bits |= s[i] & 0xff;

		if (!((i+1) % 3)) {
			guint indices[4];

			indices[0] = (bits >> 18) & 0x3f;
			indices[1] = (bits >> 12) & 0x3f;
			indices[2] = (bits >> 6) & 0x3f;
			indices[3] = bits & 0x3f;
			bits = 0;

			str[j++] = base64chars[(indices[0])];
			str[j++] = base64chars[(indices[1])];
			str[j++] = base64chars[(indices[2])];
			str[j++] = base64chars[(indices[3])];
		}
	}

	if (j + 4 < maxlen) {
		if ((i % 3) == 1) {
			guint indices[2];

			indices[0] = (bits >> 2) & 0x3f;
			indices[1] = (bits << 4) & 0x3f;

			str[j++] = base64chars[(indices[0])];
			str[j++] = base64chars[(indices[1])];
			str[j++] = '=';
			str[j++] = '=';
		} else if ((i % 3) == 2) {
			guint indices[3];

			indices[0] = (bits >> 10) & 0x3f;
			indices[1] = (bits >> 4) & 0x3f;
			indices[2] = (bits << 2) & 0x3f;

			str[j++] = base64chars[(indices[0])];
			str[j++] = base64chars[(indices[1])];
			str[j++] = base64chars[(indices[2])];
			str[j++] = '=';
		}
	}

	str[j] = '\0';

	return str;
}

struct tm *
lm_utils_get_localtime (const gchar *stamp)
{
	struct tm tm;
	time_t    t;
	gint      year, month;
	
	/* 20021209T23:51:30 */

	sscanf (stamp, "%4d%2d%2dT%2d:%2d:%2d", 
		&year, &month, &tm.tm_mday, &tm.tm_hour,
		&tm.tm_min, &tm.tm_sec);

	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_isdst = -1;

 	t = mktime (&tm);

#if defined(HAVE_TM_GMTOFF)
	t += tm.tm_gmtoff;
#elif defined(HAVE_TIMEZONE)
	t -= timezone;
	if (tm.tm_isdst > 0) {
		t += 3600;
	}
#endif	

	return localtime (&t);
}

