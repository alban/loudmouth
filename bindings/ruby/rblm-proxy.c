#include "rblm.h"

VALUE lm_cProxy;

extern void
Init_lm_proxy (VALUE lm_mLM)
{
	lm_cProxy = rb_define_class_under (lm_mLM, "Proxy", rb_cObject);
}

