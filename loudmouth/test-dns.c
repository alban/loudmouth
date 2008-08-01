#include <loudmouth.h>
#include "lm-blocking-resolver.h"
        
GMainLoop *main_loop;

static void
resolver_result_cb (LmResolver       *resolver,
                    LmResolverResult  result,
                    gpointer          user_data)
{
        gchar           *host;
        struct addrinfo *addr;

        g_object_get (resolver, "host", &host, NULL);

        if (result != LM_RESOLVER_RESULT_OK) {
                g_print ("Failed to lookup %s\n", host);
                g_free (host);
                return;
        }

        g_print ("In %s\n", G_STRFUNC);

        while ((addr = lm_resolver_results_get_next (resolver))) {
                g_print ("Result!\n");
        }

        g_free (host);

        g_main_loop_quit (main_loop);
}

int
main (int argc, char **argv)
{
#if 0
        LmResolver *resolver;
#endif
        LmResolver *srv_resolver;

        g_type_init ();
#if 0
        resolver = lm_resolver_new_for_host ("kenny.imendio.com",
                                             resolver_result_cb,
                                             NULL);
        lm_resolver_lookup (resolver);
#endif

        srv_resolver = lm_resolver_new_for_service ("jabber.org",
                                                    "xmpp-client", "tcp",
                                                    resolver_result_cb,
                                                    NULL);
        lm_resolver_lookup (srv_resolver);

        g_print ("Running main loop\n");

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);

        g_print ("Finished\n");

        return 0;
}
