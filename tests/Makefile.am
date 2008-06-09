include $(top_srcdir)/Makefile.decl

SUBDIRS = parser-tests

noinst_PROGRAMS = $(TEST_PROGS)
TEST_PROGS = 

TEST_PROGS += test-parser
test_parser_SOURCES =                         \
	test-parser.c

AM_CPPFLAGS =                                 \
	-I.                                   \
	-I$(top_srcdir)                       \
	-DLM_COMPILATION                      \
	-DRUNTIME_ENDIAN                      \
	$(LOUDMOUTH_CFLAGS)                   \
	-DPARSER_TEST_DIR="\"$(top_srcdir)/tests/parser-tests\""

LIBS =                                        \
	$(LOUDMOUTH_LIBS)                     \
	$(top_builddir)/loudmouth/libloudmouth-1.la

