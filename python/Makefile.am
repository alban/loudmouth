INCLUDES=-I$(top_builddir) $(LOUDMOUTH_CFLAGS) $(PYTHON_INCLUDES)

dbusdir = $(pythondir)
dbus_PYTHON = dbus.py

dbusbindingsdir = $(pythondir)
dbusbindings_LTLIBRARIES = loudmouth.la

loudmouth_la_LDFLAGS = -module -avoid-version -fPIC -export-symbols-regex initloudmouth
loudmouth_la_LIBADD = $(top_builddir)/loudmouth/loudmouth.la
loudmouth_la_SOURCES = loudmouth.c

EXTRA_DIST =		\
	test.py

CLEANFILES =		\
	loudmouth.pyx	\
	loudmouth.c


loudmouth.c: loudmouth.pyx
	-pyrexc loudmouth.pyx
