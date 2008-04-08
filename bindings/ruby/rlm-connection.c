#include "rloudmouth.h"

VALUE conn_set_server (VALUE self, VALUE server);

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
conn_initialize (VALUE self, VALUE server, VALUE context)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	conn_set_server (self, server);

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
	VALUE         func;

	Data_Get_Struct (self, LmConnection, conn);

	rb_scan_args (argc, argv, "0&", &func);
	if (NIL_P (func)) {
		func = rb_block_proc ();
	}

	return GBOOL2RVAL (lm_connection_open (conn, open_callback, 
					       (gpointer) func, NULL, NULL));
}

VALUE
conn_close (VALUE self)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	return GBOOL2RVAL (lm_connection_close (conn, NULL));
}

static void
auth_callback (LmConnection *conn, gboolean success, gpointer user_data)
{
	rb_funcall((VALUE)user_data, rb_intern ("call"), 1,
		   GBOOL2RVAL (success));
}

VALUE
conn_auth (int argc, VALUE *argv, VALUE self)
{
	LmConnection *conn;
	VALUE         name, password, resource, func; 

	Data_Get_Struct (self, LmConnection, conn);

	rb_scan_args (argc, argv, "21&", &name, &password, &resource, &func);
	if (NIL_P (func)) {
		func = rb_block_proc ();
	}

	return GBOOL2RVAL (lm_connection_authenticate (conn, 
						       StringValuePtr (name),
						       StringValuePtr (password), 
						       StringValuePtr (resource),
						       auth_callback,
						       (gpointer) func, NULL,
						       NULL));
}

VALUE
conn_set_keep_alive_rate (VALUE self, VALUE rate)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	lm_connection_set_keep_alive_rate (conn, NUM2UINT (rate));

	return Qnil;
}

/*
 * VALUE
conn_get_keep_alive_rate (VALUE self)
{
	LmConnection *connection;
} */

VALUE
conn_is_open (VALUE self)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);
	
	return GBOOL2RVAL (lm_connection_is_open (conn));
}

VALUE
conn_is_authenticated (VALUE self)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);
	
	return GBOOL2RVAL (lm_connection_is_authenticated (conn));
}

VALUE
conn_get_server (VALUE self)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	return rb_str_new2 (lm_connection_get_server (conn));
}

VALUE
conn_set_server (VALUE self, VALUE server)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	if (!rb_respond_to (server, rb_intern ("to_s"))) {
		rb_raise (rb_eArgError, "server should respond to to_s");
	} else {
		VALUE str_val = rb_funcall (server, rb_intern ("to_s"), 0);
		lm_connection_set_server (conn, StringValuePtr (str_val));
	}

	return Qnil;
}

VALUE
conn_get_jid (VALUE self)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	return rb_str_new2 (lm_connection_get_jid (conn));
}

VALUE
conn_set_jid (VALUE self, VALUE jid)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	if (!rb_respond_to (jid, rb_intern ("to_s"))) {
		rb_raise (rb_eArgError, "jid should respond to to_s");
	} else {
		VALUE str_val = rb_funcall (jid, rb_intern ("to_s"), 0);
		lm_connection_set_jid (conn, StringValuePtr (str_val));
	}

	return Qnil;
}

VALUE
conn_get_port (VALUE self)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	return UINT2NUM (lm_connection_get_port (conn));
}

VALUE
conn_set_port (VALUE self, VALUE port)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	lm_connection_set_port (conn, NUM2UINT (port));

	return Qnil;
}

void
Init_lm_connection (VALUE lm_mLM)
{
	VALUE lm_mConnection;
	
	lm_mConnection = rb_define_class_under (lm_mLM, "Connection", 
						rb_cObject);

	rb_define_alloc_func (lm_mConnection, conn_allocate);

	rb_define_method (lm_mConnection, "initialize", conn_initialize, 1);
	rb_define_method (lm_mConnection, "open", conn_open, -1);
	rb_define_method (lm_mConnection, "close", conn_close, 0);
	rb_define_method (lm_mConnection, "authenticate", conn_auth, -1);
	rb_define_method (lm_mConnection, "keep_alive_rate=", conn_set_keep_alive_rate, 1);
	/* rb_define_method (lm_mConnection, "keep_alive_rate", conn_get_keep_alive_rate, 0); */
	rb_define_method (lm_mConnection, "open?", conn_is_open, 0);
	rb_define_method (lm_mConnection, "authenticated?", conn_is_authenticated, 0);
	rb_define_method (lm_mConnection, "server", conn_get_server, 0);
	rb_define_method (lm_mConnection, "server=", conn_set_server, 1);
	rb_define_method (lm_mConnection, "jid", conn_get_jid, 0);
	rb_define_method (lm_mConnection, "jid=", conn_set_jid, 1);
	rb_define_method (lm_mConnection, "port", conn_get_port, 0);
	rb_define_method (lm_mConnection, "port=", conn_set_port, 1);
}
