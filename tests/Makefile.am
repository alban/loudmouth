TESTS =                                       \
	test-objects

test_objects_SOURCES =                        \
	test-objects.c                        \
	test-objects.h

if HAVE_CHECK
noinst_PROGRAMS = $(TESTS)
endif

AM_CPPFLAGS =                                 \
	-I.                                   \
	-I$(top_srcdir)                       \
	-DLM_COMPILATION                      \
	-DRUNTIME_ENDIAN                      \
	$(LM_CFLAGS)                          \
	@CHECK_CFLAGS@

LIBS =                                        \
	$(LM_LIBS)                            \
	@CHECK_LIBS@                          \
	$(top_builddir)/loudmouth/libloudmouth-1.la

