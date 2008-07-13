include $(top_srcdir)/Makefile.decl

NULL=

TEST_DIR=$(top_srcdir)/

AM_CPPFLAGS = 				    \
	-I.				    \
	-I$(top_srcdir)			    \
	$(LOUDMOUTH_CFLAGS)		    \
	$(LIBIDN_CFLAGS)		    \
	-DLM_COMPILATION	  	    \
	-DRUNTIME_ENDIAN                    \
	$(NULL)

lib_LTLIBRARIES = libloudmouth-1.la

if USE_GNUTLS
ssl_sources =                           \
	lm-ssl-gnutls.c
endif

if USE_OPENSSL
ssl_sources =                           \
	lm-ssl-openssl.c
endif

libloudmouth_1_la_SOURCES =		\
	lm-connection.c	 		\
	lm-debug.c                      \
	lm-debug.h                      \
	lm-dummy.c                      \
	lm-dummy.h                      \
	lm-error.c			\
	lm-marshal-main.c               \
	lm-message.c	 		\
	lm-message-handler.c		\
	lm-message-node.c		\
	lm-message-queue.c		\
	lm-message-queue.h		\
	lm-misc.c                       \
	lm-misc.h                       \
	lm-parser.c			\
	lm-parser.h			\
	lm-resolver.c                   \
	lm-resolver.h                   \
	lm-internals.h			\
	lm-sha.c			\
	lm-sha.h			\
	lm-ssl-generic.c                \
	lm-ssl-base.c                   \
	lm-ssl-base.h                   \
	lm-ssl-internals.h              \
	$(ssl_sources)                  \
	lm-utils.c			\
	lm-proxy.c                      \
	lm-sock.h			\
	lm-sock.c			\
	lm-socket.c                     \
	lm-socket.h                     \
	lm-soup-socket.c                \
	lm-soup-socket.h                \
	lm-tcp-socket.c                 \
	lm-tcp-socket.h                 \
	lm-old-socket.c                 \
	lm-old-socket.h                 \
	asyncns.c                       \
	asyncns.h                       \
	lm-sasl.c                       \
	lm-sasl.h                       \
	lm-simple-io.c                  \
	lm-simple-io.h                  \
	lm-xmpp-writer.c                \
	lm-xmpp-writer.h                \
	md5.c                           \
	md5.h                           \
	$(NULL)

libloudmouthinclude_HEADERS =		\
	lm-connection.h			\
	lm-error.h			\
	lm-message.h		 	\
	lm-message-handler.h		\
	lm-message-node.h		\
	lm-utils.h			\
	lm-proxy.h                      \
	lm-ssl.h                        \
	loudmouth.h			\
	$(NULL)

libloudmouth_1_la_LIBADD = 		\
	$(LOUDMOUTH_LIBS)		\
	$(LIBIDN_LIBS) \
	-lresolv

libloudmouth_1_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols $(srcdir)/loudmouth.sym
libloudmouthincludedir = $(includedir)/loudmouth-1.0/loudmouth

lm-marshal.h: lm-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=lm_marshal lm-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh lm-marshal.h || cp xgen-gmh lm-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

lm-marshal.c: lm-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=lm_marshal lm-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc lm-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

lm-marshal-main.c: lm-marshal.c lm-marshal.h

BUILT_SOURCES =				\
	lm-marshal.h			\
	lm-marshal.c

# an explicit dependency here so all generated files get built
$(OBJECTS): $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST += \
	lm-ssl-gnutls.c \
	lm-ssl-openssl.c \
	loudmouth.sym \
	lm-marshal.list

