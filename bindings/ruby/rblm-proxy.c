#include "rblm.h"

VALUE lm_cProxy;

static LmProxy *
rb_lm_proxy_from_ruby_object (VALUE obj)
{
	LmProxy *proxy;

	if (!rb_lm__is_kind_of (obj, lm_cProxy)) {
		rb_raise (rb_eTypeError, "no a LmProxy");
	}

	Data_Get_Struct (obj, LmProxy, proxy);

	return proxy;
}

void
proxy_free (LmProxy *proxy)
{
	lm_proxy_unref (proxy);
}

VALUE
proxy_allocate (VALUE klass)
{
	return Data_Wrap_Struct (klass, NULL, proxy_free, NULL);
}

VALUE
proxy_initialize (int argc, VALUE *argv, VALUE self)
{
	LmProxy *proxy;
	VALUE    type, server, port;
	char    *server_str = NULL;

	rb_scan_args (argc, argv, "12", &type, &server, &port);

	proxy = lm_proxy_new (FIX2INT (type));

	if (!NIL_P (server)) {
		if (!rb_respond_to (server, rb_intern ("to_s"))) {
			rb_raise (rb_eArgError, "server should respond to to_s");
		} else {
			VALUE str_val = rb_funcall (server, rb_intern ("to_s"), 0);
			lm_proxy_set_server (proxy, StringValuePtr (str_val));
		}
	}

	if (!NIL_P (port)) {
		lm_proxy_set_port (proxy, NUM2UINT (port));
	}

	DATA_PTR (self) = proxy;

	return self;
}

VALUE
proxy_get_type (VALUE self)
{
	LmProxy *proxy = rb_lm_proxy_from_ruby_object (self);

	return INT2FIX (lm_proxy_get_type (proxy));
}

VALUE
proxy_set_type (VALUE self, VALUE type)
{
	LmProxy *proxy = rb_lm_proxy_from_ruby_object (self);

	lm_proxy_set_type (proxy, FIX2INT (type));

	return Qnil;
}

extern void
Init_lm_proxy (VALUE lm_mLM)
{
	lm_cProxy = rb_define_class_under (lm_mLM, "Proxy", rb_cObject);

	rb_define_alloc_func (lm_cProxy, proxy_allocate);

	rb_define_method (lm_cProxy, "initialize", proxy_initialize, -1);
	rb_define_method (lm_cProxy, "type", proxy_get_type, 0);
	rb_define_method (lm_cProxy, "type=", proxy_set_type, 1);
}

