if HAVE_MONO
    MONO_SUBDIR = mono
endif

SUBDIRS = loudmouth docs $(MONO_SUBDIR)

EXTRA_DIST =			\
	loudmouth-1.0.pc.in     \
        loudmouth.spec          \
	loudmouth.spec.in       \
	README.WIN32

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = loudmouth-1.0.pc

