#include "rblm.h"

VALUE lm_mMessageType;
VALUE lm_mMessageSubType;
VALUE lm_mDisconnectReason;
VALUE lm_mConnectionState;

LmConnectionState
rb_lm_connection_state_from_ruby_object (VALUE obj)
{
	LmConnectionState state;

	state = FIX2INT (obj);
	if (state < LM_CONNECTION_STATE_CLOSED ||
	    state > LM_CONNECTION_STATE_AUTHENTICATED) {
		rb_raise (rb_eArgError,
			  "invalid LmConnectionState: %d (expected %d <= LmConnectionState <= %d)",
			  state, LM_CONNECTION_STATE_CLOSED, 
			  LM_CONNECTION_STATE_AUTHENTICATED);
	}

	return state;
}

LmDisconnectReason
rb_lm_disconnect_reason_from_ruby_object (VALUE obj)
{
	LmDisconnectReason reason;

	reason = FIX2INT (obj);
	if (reason < LM_DISCONNECT_REASON_OK ||
	    reason > LM_DISCONNECT_REASON_UNKNOWN) {
		rb_raise (rb_eArgError,
			  "invalid LmDisconnectReason: %d (expected %d <= LmDisconnectReason <= %d)",
			  reason, 
			  LM_DISCONNECT_REASON_OK, 
			  LM_DISCONNECT_REASON_UNKNOWN);
	}

	return reason;
}

LmMessageType
rb_lm_message_type_from_ruby_object (VALUE obj)
{
	LmMessageType type;

	type = FIX2INT (obj);
	if (type < LM_MESSAGE_TYPE_MESSAGE ||
	    type > LM_MESSAGE_TYPE_IQ) {
		rb_raise (rb_eArgError,
			  "invalid LmMessageType: %d (expected %d <= LmMessageType <= %d)",
			  type, LM_MESSAGE_TYPE_MESSAGE,
			  LM_MESSAGE_TYPE_IQ);
	}

	return type;
}

LmMessageSubType
rb_lm_message_sub_type_from_ruby_object (VALUE obj)
{
	LmMessageSubType type;

	type = FIX2INT (obj);
	if (type < LM_MESSAGE_SUB_TYPE_AVAILABLE ||
	    type > LM_MESSAGE_SUB_TYPE_ERROR) {
		rb_raise (rb_eArgError,
			  "invalid LmMessageSubType: %d (expected %d <= LmMessageSubType <= %d)",
			  type, LM_MESSAGE_SUB_TYPE_AVAILABLE,
			  LM_MESSAGE_SUB_TYPE_ERROR);
	}

	return type;
}

void 
Init_lm_constants (VALUE lm_mLM)
{
	/* LmMessageType */
	lm_mMessageType = rb_define_module_under (lm_mLM, "MessageType");

	rb_define_const (lm_mMessageType, "MESSAGE",
			 INT2FIX (LM_MESSAGE_TYPE_MESSAGE));
	rb_define_const (lm_mMessageType, "PRESENCE",
			 INT2FIX (LM_MESSAGE_TYPE_PRESENCE));
	rb_define_const (lm_mMessageType, "IQ",
			 INT2FIX (LM_MESSAGE_TYPE_IQ));

	/* LmMessageSubType */
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

	/* LmDisconnectReason */
	lm_mDisconnectReason = rb_define_module_under (lm_mLM, "DisconnectReason");

	rb_define_const (lm_mDisconnectReason, "OK",
			 INT2FIX (LM_DISCONNECT_REASON_OK));
	rb_define_const (lm_mDisconnectReason, "PING_TIME_OUT",
			 INT2FIX (LM_DISCONNECT_REASON_PING_TIME_OUT));
	rb_define_const (lm_mDisconnectReason, "HUP",
			 INT2FIX (LM_DISCONNECT_REASON_HUP));
	rb_define_const (lm_mDisconnectReason, "ERROR",
			 INT2FIX (LM_DISCONNECT_REASON_ERROR));
	rb_define_const (lm_mDisconnectReason, "RESOURCE_CONFLICT",
			 INT2FIX (LM_DISCONNECT_REASON_RESOURCE_CONFLICT));
	rb_define_const (lm_mDisconnectReason, "INVALID_XML",
			 INT2FIX (LM_DISCONNECT_REASON_INVALID_XML));
	rb_define_const (lm_mDisconnectReason, "UNKNOWN",
			 INT2FIX (LM_DISCONNECT_REASON_UNKNOWN));

	/* LmConnectionState */
	lm_mConnectionState = rb_define_module_under (lm_mLM, "ConnectionState");
	rb_define_const (lm_mConnectionState, "CLOSED",
			 INT2FIX (LM_CONNECTION_STATE_CLOSED));
	rb_define_const (lm_mConnectionState, "OPENING",
			 INT2FIX (LM_CONNECTION_STATE_OPENING));
	rb_define_const (lm_mConnectionState, "OPEN",
			 INT2FIX (LM_CONNECTION_STATE_OPEN));
	rb_define_const (lm_mConnectionState, "AUTHENTICATING",
			 INT2FIX (LM_CONNECTION_STATE_AUTHENTICATING));
	rb_define_const (lm_mConnectionState, "AUTHENTICATED",
			 INT2FIX (LM_CONNECTION_STATE_AUTHENTICATED));

}
