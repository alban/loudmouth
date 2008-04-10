#include "rblm.h"

VALUE lm_cSSL;

extern void
Init_lm_ssl (VALUE lm_mLM)
{
	lm_cSSL = rb_define_class_under (lm_mLM, "SSL", rb_cObject);
}

