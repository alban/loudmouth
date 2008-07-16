#include <loudmouth.h>
#include "lm-blocking-resolver.h"

int
main (int argc, char **argv)
{
        LmResolver *resolver;
        GMainLoop  *main_loop;
        LmResolver *srv_resolver;

        g_type_init ();

        resolver = g_object_new (LM_TYPE_BLOCKING_RESOLVER,
                                 "type", LM_RESOLVER_HOST,
                                 "host", "kenny.imendio.com",
                                 NULL);

        lm_resolver_lookup (resolver);

        srv_resolver = g_object_new (LM_TYPE_BLOCKING_RESOLVER,
                                     "type", LM_RESOLVER_SRV,
                                     "domain", "jabber.org",
                                     "protocol", "tcp",
                                     "service", "xmpp-client",
                                     NULL);

        lm_resolver_lookup (srv_resolver);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);

        return 0;
}
