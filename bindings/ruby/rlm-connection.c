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
conn_initialize(VALUE self, VALUE server)
{
	LmConnection *conn;

	Data_Get_Struct (self, LmConnection, conn);

	if (!rb_respond_to (server, rb_intern ("to_s"))) {
		rb_raise (rb_eArgError, "server should respond to to_s");
	} else {
		VALUE str_val = rb_funcall (server, rb_intern ("to_s"), 0);
		lm_connection_set_server (conn, StringValuePtr (str_val));
	}

	return self;
}

VALUE
conn_open (VALUE block)
{
	return Qtrue;
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
