MCS=mcs
PREFIX=/opt/gnome2
DESTDIR=

all: linux

linux: loudmouth-sharp.dll

loudmouth-sharp.dll: *.cs
	$(MCS) --target library -o loudmouth-sharp.dll --recurse '*.cs'

clean:
	rm -f *.dll

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/lib && \
	cp loudmouth-sharp.dll $(DESTDIR)$(PREFIX)/lib

