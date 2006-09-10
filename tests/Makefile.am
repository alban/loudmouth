SUBDIRS = parser-tests

TESTS =                                       \
	test-objects                          \
	test-parser

test_objects_SOURCES =                        \
	test-objects.c                        \
	test-objects.h

test_parser_SOURCES =                         \
	test-parser.c

if HAVE_CHECK
noinst_PROGRAMS = $(TESTS)
endif

AM_CPPFLAGS =                                 \
	-I.                                   \
	-I$(top_srcdir)                       \
	-DLM_COMPILATION                      \
	-DRUNTIME_ENDIAN                      \
	$(LOUDMOUTH_CFLAGS)                   \
	-DPARSER_TEST_DIR="\"$(top_srcdir)/tests/parser-tests\""  \
	@CHECK_CFLAGS@

LIBS =                                        \
	$(LOUDMOUTH_LIBS)                     \
	@CHECK_LIBS@                          \
	$(top_builddir)/loudmouth/libloudmouth-1.la

