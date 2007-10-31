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
	lm-error.c			\
	lm-message.c	 		\
	lm-message-handler.c		\
	lm-message-node.c		\
	lm-message-queue.c		\
	lm-message-queue.h		\
	lm-misc.c                       \
	lm-misc.h                       \
	lm-parser.c			\
	lm-parser.h			\
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
	asyncns.c     \
	asyncns.h     \
	lm-sasl.c                       \
	lm-sasl.h                       \
	md5.c                           \
	md5.h                           \
	base64.c                        \
	base64.h                        \
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

# an explicit dependency here so alm generated files get built
$(OBJECTS): $(built_sources)

EXTRA_DIST = \
	lm-ssl-gnutls.c \
	lm-ssl-openssl.c \
	loudmouth.sym

