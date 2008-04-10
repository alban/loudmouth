#include <rblm.h>

VALUE lm_cMessageNode;

void
msg_node_free (LmMessageNode *node)
{
	lm_message_node_unref (node);
}

VALUE
msg_node_allocate (VALUE klass)
{
	return Data_Wrap_Struct (klass, NULL, msg_node_free, NULL);
}

extern void 
Init_lm_message_node (VALUE lm_mLM)
{
	lm_cMessageNode = rb_define_class_under (lm_mLM, "MessageNode", 
						 rb_cObject);

	rb_define_alloc_func (lm_cMessageNode, msg_node_allocate);
}	
