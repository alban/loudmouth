NULL=

INCLUDES = 				\
	-I.				\
	-I$(top_srcdir)			\
	$(LOUDMOUTH_CFLAGS)		\
	-DLM_COMPILATION		\
	-DRUNTIME_ENDIAN                \
	$(NULL)

lib_LTLIBRARIES = libloudmouth-1.la

libloudmouth_1_la_SOURCES =		\
	lm-connection.c	 		\
	lm-debug.c                      \
	lm-debug.h                      \
	lm-error.c			\
	lm-message.c	 		\
	lm-message-handler.c		\
	lm-message-node.c		\
	lm-parser.c			\
	lm-parser.h			\
	lm-internals.h			\
	lm-sha.c			\
	lm-sha.h			\
	lm-ssl.c                        \
	lm-utils.c			\
	lm-proxy.c                      \
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
	$(LOUDMOUTH_LIBS)

libloudmouthincludedir = $(includedir)/loudmouth-1.0/loudmouth

# an explicit dependency here so alm generated files get built
$(OBJECTS): $(built_sources)
