/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#ifndef __LM_DUMMY_H__
#define __LM_DUMMY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LM_TYPE_DUMMY            (lm_dummy_get_type ())
#define LM_DUMMY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LM_TYPE_DUMMY, LmDummy))
#define LM_DUMMY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LM_TYPE_DUMMY, LmDummyClass))
#define LM_IS_DUMMY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LM_TYPE_DUMMY))
#define LM_IS_DUMMY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LM_TYPE_DUMMY))
#define LM_DUMMY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LM_TYPE_DUMMY, LmDummyClass))

typedef struct LmDummy      LmDummy;
typedef struct LmDummyClass LmDummyClass;

struct LmDummy {
	GObject parent;
};

struct LmDummyClass {
	GObjectClass parent_class;
	
	/* <vtable> */
	void  (*initialize)    (LmDummy     *dummy,
				const char *username,
				const char *server,
				const char *password);
	void  (*begin)         (LmDummy     *dummy);
	void  (*cancel)        (LmDummy     *dummy);
};

GType   lm_dummy_get_type  (void);

G_END_DECLS

#endif /* __LM_DUMMY_H__ */

