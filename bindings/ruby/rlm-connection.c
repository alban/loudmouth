#include "rloudmouth.h"

void
conn_mark (LmConnection *self)
{
}

void
conn_free (LmConnection *self)
{
	lm_connection_unref (self);
}

VALUE
conn_allocate(VALUE klass)
{
	LmConnection *conn = lm_connection_new (NULL);

	return Data_Wrap_Struct (klass, conn_mark, conn_free, conn);
}

VALUE
conn_initialize(VALUE self, VALUE server, VALUE context)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	if (!rb_respond_to (server, rb_intern ("to_s"))) {
		rb_raise (rb_eArgError, "server should respond to to_s");
	} else {
		VALUE str_val = rb_funcall (server, rb_intern ("to_s"), 0);
		lm_connection_set_server (conn, StringValuePtr (str_val));
	}

	/* Set context */

	return self;
}

static void
open_callback (LmConnection *conn, gboolean success, gpointer user_data)
{
	rb_funcall((VALUE)user_data, rb_intern ("call"), 1,
		   GBOOL2RVAL (success));
}

VALUE
conn_open (int argc, VALUE *argv, VALUE self)
{
	LmConnection *conn;
	VALUE         ret_val;
	VALUE         func;

	Data_Get_Struct (self, LmConnection, conn);

	rb_scan_args (argc, argv, "&", &func);
	if (NIL_P (func)) {
		func = rb_block_proc ();
	}

	return GBOOL2RVAL (lm_connection_open (conn, open_callback, 
					       (gpointer) func, NULL, NULL));
}

void
Init_lm_connection (VALUE lm_mLM)
{
	VALUE lm_mConnection;
	
	lm_mConnection = rb_define_class_under (lm_mLM, "Connection", 
						rb_cObject);

	rb_define_alloc_func (lm_mConnection, conn_allocate);

	rb_define_method (lm_mConnection, "initialize", conn_initialize, 1);
	rb_define_method (lm_mConnection, "open", conn_open, 1);
}
