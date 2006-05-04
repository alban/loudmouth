SUBDIRS = loudmouth docs examples tests

EXTRA_DIST =			\
	loudmouth-1.0.pc.in     \
        loudmouth.spec          \
	loudmouth.spec.in       \
	CONTRIBUTORS            \
	README.WIN32

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = loudmouth-1.0.pc

DISTCHECK_CONFIGURE_FLAGS =     \
	--enable-gtk-doc

