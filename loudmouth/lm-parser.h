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

#ifndef __LM_PARSER_H__
#define __LM_PARSER_H__

#include <glib.h>
#include "lm-message.h"

typedef struct LmParser LmParser;

typedef void (* LmParserMessageFunction) (LmParser     *parser,
					  LmMessage    *message,
					  gpointer      user_data);

LmParser *   lm_parser_new       (LmParserMessageFunction  function,
				  gpointer                 user_data,
				  GDestroyNotify           notify);
gboolean     lm_parser_parse     (LmParser                *parser,
				  const gchar             *string);
void         lm_parser_free      (LmParser                *parser);

#endif /* __LM_PARSER_H__ */
