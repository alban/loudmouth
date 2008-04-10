#include "rblm.h"

VALUE lm_cSSL;

static LmSSL *
rb_lm_ssl_from_ruby_object (VALUE obj)
{
	LmSSL *ssl;

	if (!rb_lm__is_kind_of (obj, lm_cSSL)) {
		rb_raise (rb_eTypeError, "not a LmSSL");
	}

	Data_Get_Struct (obj, LmSSL, ssl);

	return ssl;
}

void
ssl_free (LmSSL *ssl)
{
	lm_ssl_unref (ssl);
}

VALUE
ssl_allocate (VALUE klass)
{
	return Data_Wrap_Struct (klass, NULL, ssl_free, NULL);
}

static LmSSLResponse
ssl_func_callback (LmSSL       *ssl,
		   LmSSLStatus  status,
		   gpointer     user_data)
{
	VALUE response;

	if (!user_data) {
		return LM_SSL_RESPONSE_CONTINUE;
	}

	response = rb_funcall ((VALUE)user_data, rb_intern ("call"), 1,
			       INT2FIX (status));

	return rb_lm_ssl_response_from_ruby_object (response);
}

VALUE
ssl_initialize (int argc, VALUE *argv, VALUE self)
{
	LmSSL    *ssl;
	VALUE     fingerprint;
	VALUE     func;
	char     *fingerprint_str = NULL;
	gpointer  func_ptr = NULL;

	rb_scan_args (argc, argv, "01&", &fingerprint, &func);

	if (!NIL_P (func)) {
		func_ptr = (gpointer) func;
	}

	if (!NIL_P (fingerprint)) {
		VALUE str_val;

		if (!rb_respond_to (fingerprint, rb_intern ("to_s"))) {
			rb_raise (rb_eArgError, 
				  "fingerprint should respond to to_s");
		}

		str_val = rb_funcall (fingerprint, rb_intern ("to_s"), 0);
		fingerprint_str = StringValuePtr (str_val);
	}

	ssl = lm_ssl_new (fingerprint_str, ssl_func_callback,
			  func_ptr, NULL);

	DATA_PTR (self) = ssl;

	return self;
}

extern void
Init_lm_ssl (VALUE lm_mLM)
{
	lm_cSSL = rb_define_class_under (lm_mLM, "SSL", rb_cObject);

	rb_define_alloc_func (lm_cSSL, ssl_allocate);

	rb_define_method (lm_cSSL, "initialize", ssl_initialize, -1);
}

