#include <rblm.h>

VALUE lm_cMessageNode;

static LmMessageNode *
rb_lm_message_node_from_ruby_object (VALUE obj)
{
	LmMessageNode *node;

	if (!rb_lm__is_kind_of (obj, lm_cMessageNode)) {
		rb_raise (rb_eTypeError, "not a LmMessageNode");
	}

	Data_Get_Struct (obj, LmMessageNode, node);

	return node;
}

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

VALUE
msg_node_get_value (VALUE self)
{
	LmMessageNode *node = rb_lm_message_node_from_ruby_object (self);

	if (lm_message_node_get_value (node)) {
		return rb_str_new2 (lm_message_node_get_value (node));
	} 

	return Qnil;
}

VALUE
msg_node_set_value (VALUE self, VALUE value)
{
	LmMessageNode *node = rb_lm_message_node_from_ruby_object (self);	
	char          *value_str = NULL;
	
	if (!rb_respond_to (value, rb_intern ("to_s"))) {
		rb_raise (rb_eArgError, "value should respond to to_s");
	} else {
		VALUE str_val = rb_funcall (value, rb_intern ("to_s"), 0);
		value_str = StringValuePtr (str_val);
	}

	lm_message_node_set_value (node, value_str);
}

VALUE
msg_node_add_child (int argc, VALUE *argv, VALUE self)
{
	return Qnil;
}

VALUE
msg_node_get_attribute (VALUE self, VALUE attr)
{
	return Qnil;
}

VALUE
msg_node_set_attribute (VALUE self, VALUE attr, VALUE value)
{
	return Qnil;
}

VALUE 
msg_node_get_child (VALUE self, VALUE name)
{
	return Qnil;
}

VALUE
msg_node_find_child (VALUE self, VALUE name)
{
	return Qnil;
}

VALUE
msg_node_get_is_raw_mode (VALUE self)
{
	return Qnil;
}

VALUE
msg_node_set_is_raw_mode (VALUE self, VALUE raw_mode)
{
	return Qnil;
}

VALUE
msg_node_to_string (VALUE self)
{
	return Qnil;
}

extern void 
Init_lm_message_node (VALUE lm_mLM)
{
	lm_cMessageNode = rb_define_class_under (lm_mLM, "MessageNode", 
						 rb_cObject);

	rb_define_alloc_func (lm_cMessageNode, msg_node_allocate);

	rb_define_method (lm_cMessageNode, "value", msg_node_get_value, 0);
	rb_define_method (lm_cMessageNode, "value=", msg_node_set_value, 1);

	rb_define_method (lm_cMessageNode, "add_child", msg_node_add_child, -1);
	rb_define_method (lm_cMessageNode, "get_attribute", msg_node_get_attribute, 1);
	rb_define_method (lm_cMessageNode, "set_attribute", msg_node_set_attribute, 2);
	rb_define_method (lm_cMessageNode, "get_child", msg_node_get_child, 1);
	rb_define_method (lm_cMessageNode, "find_child", msg_node_find_child, 1);

	rb_define_method (lm_cMessageNode, "raw_mode", msg_node_get_is_raw_mode, 0);
	rb_define_method (lm_cMessageNode, "raw_mode=", msg_node_set_is_raw_mode, 1);

	rb_define_method (lm_cMessageNode, "to_s", msg_node_to_string, 0);
}	
