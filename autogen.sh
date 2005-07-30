#!/bin/sh

PACKAGE="loudmouth"

have_libtool=false
have_autoconf=false
have_automake=false
need_configure_in=false

have_gtk_doc=false
want_gtk_doc=false

if libtool --version < /dev/null > /dev/null 2>&1 ; then
	libtool_version=`libtoolize --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
	have_libtool=true
	case $libtool_version in
	    1.3*)
		need_configure_in=true
		;;
	esac
fi

if autoconf --version < /dev/null > /dev/null 2>&1 ; then
	autoconf_version=`autoconf --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
	have_autoconf=true
	case $autoconf_version in
	    2.13)
		need_configure_in=true
		;;
	esac
fi

if $have_libtool ; then : ; else
	echo;
	echo "You must have libtool >= 1.3 installed to compile $PACKAGE";
	echo;
	exit;
fi

if grep "^GTK_DOC_CHECK" ./configure.in; then
	want_gtk_doc=true
fi

if $want_gtk_doc; then
	(gtkdocize --version) < /dev/null > /dev/null 2>&1 || {
	        echo;
		echo "You need gtk-doc to build $PACKAGE";
		echo;
	}
fi

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have automake installed to compile $PACKAGE";
	echo;
	exit;
}

echo "Generating configuration files for $PACKAGE, please wait...."
echo;

if $need_configure_in ; then
    if test ! -f configure.in ; then
	echo "Creating symlink from configure.in to configure.ac..."
	echo
	ln -s configure.ac configure.in
    fi
fi

aclocal $ACLOCAL_FLAGS
libtoolize --force
gtkdocize || exit 1
autoheader
automake --add-missing
autoconf

./configure $@ --enable-maintainer-mode --enable-compile-warnings

