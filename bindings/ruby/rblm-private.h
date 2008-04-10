
#ifndef __RBLM_PRIVATE_H__
#define __RBLM_PRIVATE_H__

#include <glib.h>
#include <ruby.h>

#define GBOOL2RVAL(x) (x == TRUE ? Qtrue : Qfalse)
#define RVAL2GBOOL(x) RTEST(x)

gboolean            rb_lm__is_kind_of (VALUE object, VALUE klass);

LmConnectionState   rb_lm_connection_state_from_ruby_object   (VALUE obj);
LmDisconnectReason  rb_lm_disconnect_reason_from_ruby_object  (VALUE obj);
LmMessageType       rb_lm_message_type_from_ruby_object       (VALUE obj);
LmMessageSubType    rb_lm_message_sub_type_from_ruby_object   (VALUE obj);
LmProxyType         rb_lm_proxy_type_from_ruby_object         (VALUE obj);
LmCertificateStatus rb_lm_certificate_status_from_ruby_object (VALUE obj);
LmSSLStatus         rb_lm_ssl_status_from_ruby_object         (VALUE obj);
LmSSLResponse       rb_lm_ssl_response_from_ruby_object       (VALUE obj);

#endif /* __RBLM_PRIVATE_H__ */

