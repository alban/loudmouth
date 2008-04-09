#include "rblm.h"

VALUE lm_mMessageType;
VALUE lm_mMessageSubType;

void 
Init_lm_constants (VALUE lm_mLM)
{
	lm_mMessageType = rb_define_module_under (lm_mLM, "MessageType");

	rb_define_const (lm_mMessageType, "MESSAGE",
			 INT2FIX (LM_MESSAGE_TYPE_MESSAGE));
	rb_define_const (lm_mMessageType, "PRESENCE",
			 INT2FIX (LM_MESSAGE_TYPE_PRESENCE));
	rb_define_const (lm_mMessageType, "IQ",
			 INT2FIX (LM_MESSAGE_TYPE_IQ));

	lm_mMessageSubType = rb_define_module_under (lm_mLM, "MessageSubType");

	rb_define_const (lm_mMessageSubType, "AVAILABLE",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_AVAILABLE));
	rb_define_const (lm_mMessageSubType, "NORMAL",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_NORMAL));
	rb_define_const (lm_mMessageSubType, "CHAT",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_CHAT));
	rb_define_const (lm_mMessageSubType, "GROUPCHAT",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_GROUPCHAT));
	rb_define_const (lm_mMessageSubType, "HEADLINE",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_HEADLINE));
	rb_define_const (lm_mMessageSubType, "UNAVAILABLE",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_UNAVAILABLE));
	rb_define_const (lm_mMessageSubType, "PROBE",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_PROBE));
	rb_define_const (lm_mMessageSubType, "SUBSCRIBE",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_SUBSCRIBE));
	rb_define_const (lm_mMessageSubType, "UNSUBSCRIBE",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE));
	rb_define_const (lm_mMessageSubType, "SUBSCRIBED",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_SUBSCRIBED));
	rb_define_const (lm_mMessageSubType, "UNSUBSCRIBED",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED));
	rb_define_const (lm_mMessageSubType, "GET",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_GET));
	rb_define_const (lm_mMessageSubType, "SET",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_SET));
	rb_define_const (lm_mMessageSubType, "RESULT",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_RESULT));
	rb_define_const (lm_mMessageSubType, "ERROR",
			 INT2FIX (LM_MESSAGE_SUB_TYPE_ERROR));
}
