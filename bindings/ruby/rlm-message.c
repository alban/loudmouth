#include "rloudmouth.h"

VALUE
msg_allocate (VALUE klass)
{
}

extern void 
Init_lm_message (VALUE lm_mLM)
{
	VALUE lm_mConnection;

	lm_mMessage = rb_define_class_under (lm_mLM, "Message", 
					     rb_cObject);

	rb_define_alloc_func (lm_mMessage, msg_allocate);
	
	rb_define_method (lm_mMessage, "initialize", msg_initialize, 1);
}

