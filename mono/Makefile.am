LOUDMOUTH_CSFILES =                                \
		Account.cs                         \
		Connection.cs                      \
		Elements.cs                        \
		ElementNode.cs                     \
		Object.cs

loudmouthlibdir = $(prefix)/lib

loudmouth-sharp.dll: $(LOUDMOUTH_CSFILES)
	$(MCS) /target:library $(LOUDMOUTH_CSFILES) -o loudmouth-sharp.dll

all: loudmouth-sharp.dll

install-data-local: loudmouth-sharp.dll
	$(mkinstalldirs) $(DESTDIR)$(loudmouthlibdir)
	$(INSTALL_DATA) loudmouth-sharp.dll $(DESTDIR)$(loudmouthlibdir)/loudmouth-sharp.dll

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = loudmouth-sharp.pc

EXTRA_DIST = \
	     $(LOUDMOUTH_CSFILES)    \
	     loudmouth-sharp.pc.in

CLEANFILES = loudmouth-sharp.dll


		
