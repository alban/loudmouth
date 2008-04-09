#include "rblm.h"

void
Init_loudmouth (void)
{
	VALUE lm_mLM;
	
	lm_mLM = rb_define_module ("LM");

	Init_lm_connection (lm_mLM);
	Init_lm_message (lm_mLM);
	Init_lm_constants (lm_mLM);
}

