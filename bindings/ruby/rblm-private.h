
#ifndef __RBLM_PRIVATE_H__
#define __RBLM_PRIVATE_H__

#include <glib.h>
#include <ruby.h>

#define GBOOL2RVAL(x) (x == TRUE ? Qtrue : Qfalse)
#define RVAL2GBOOL(x) RTEST(x)

gboolean   rb_lm__is_kind_of (VALUE object, VALUE klass);

#endif /* __RBLM_PRIVATE_H__ */

