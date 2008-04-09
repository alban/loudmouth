#include "rloudmouth.h"

/* How to handle type, sub_type and root node*/

typedef struct {
	LmMessage *message;
} MsgWrapper;

void
msg_wrapper_mark (MsgWrapper *wrapper)
{
	/* Do nothing here */
}

void
msg_wrapper_free (MsgWrapper *wrapper)
{
	if (wrapper->message) {
		lm_message_unref (wrapper->message);
	}

	g_slice_free (MsgWrapper, wrapper);
}

VALUE
msg_allocate (VALUE klass)
{
	MsgWrapper *wrapper;

	wrapper = g_slice_new0 (MsgWrapper);

	Data_Wrap_Struct (klass, msg_wrapper_mark, msg_wrapper_free, wrapper);
}

VALUE
msg_initialize (int argc, VALUE *argv, VALUE self)
{
	MsgWrapper *wrapper;
	VALUE       to, type, sub_type;
	char       *to_str = NULL;

	Data_Get_Struct (self, MsgWrapper, wrapper);

	rb_scan_args (argc, argv, "21", &to, &type, &sub_type);

	/* To can be nil */
	if (!NIL_P (to)) {
		if (!rb_respond_to (to, rb_intern ("to_s"))) {
			rb_raise (rb_eArgError, "to should respond to to_s");
		} else {
			VALUE str_val = rb_funcall (to, rb_intern ("to_s"), 0);
			to_str = StringValuePtr (str_val);
		}
	} 

	if (NIL_P (sub_type)) {
		/* Without sub_type */
		wrapper->message = lm_message_new (to_str, NUM2INT (type));
	} else {
		wrapper->message = 
			lm_message_new_with_sub_type (to_str,
						      NUM2INT (type),
						      NUM2INT (sub_type));
	}

	return self;
}

VALUE
msg_get_type (VALUE self)
{
	MsgWrapper *wrapper;

	Data_Get_Struct (self, MsgWrapper, wrapper);

	return INT2NUM (lm_message_get_type (wrapper->message));
}

VALUE
msg_get_sub_type (VALUE self)
{
	MsgWrapper *wrapper;

	Data_Get_Struct (self, MsgWrapper, wrapper);

	return INT2NUM (lm_message_get_sub_type (wrapper->message));
}

VALUE
msg_get_root_node (VALUE self)
{
	/* How to? */
	return Qnil;
}

LmMessage *
rlm_message_from_value (VALUE self)
{
	MsgWrapper *wrapper;

	Data_Get_Struct (self, MsgWrapper, wrapper);

	return wrapper->message;
}

extern void 
Init_lm_message (VALUE lm_mLM)
{
	VALUE lm_cMessage;

	lm_cMessage = rb_define_class_under (lm_mLM, "Message", rb_cObject);

	rb_define_alloc_func (lm_cMessage, msg_allocate);
	
	rb_define_method (lm_cMessage, "initialize", msg_initialize, -1);
	rb_define_method (lm_cMessage, "type", msg_get_type, 0);
	rb_define_method (lm_cMessage, "sub_type", msg_get_sub_type, 0);
	rb_define_method (lm_cMessage, "root_node", msg_get_root_node, 0);
}

