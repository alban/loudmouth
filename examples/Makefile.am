INCLUDES =                                         \
	-I$(top_srcdir)                            \
	$(LOUDMOUTH_CFLAGS)

noinst_PROGRAMS =                                  \
	test-lm                                    \
	test-http-proxy                            \
	lm-send-sync                               \
	lm-send-async                             

test_lm_SOURCES = test-lm.c

test_http_proxy_SOURCES = test-http-proxy.c

lm_send_sync_SOURCES = lm-send-sync.c

lm_send_async_SOURCES = lm-send-async.c

LDADD =                                            \
	$(LOUDMOUTH_LIBS)                          \
	$(top_builddir)/loudmouth/libloudmouth-1.la

	
