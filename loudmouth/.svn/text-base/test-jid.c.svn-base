#include "lm-jid.h"

int 
main (int argc, char **argv)
{
        int i;
        
        if (argc < 2) {
                g_print ("Usage: %s <jid> <jid> ...\n", argv[0]);
                exit (1);
        }

        for (i = 1; i < argc; ++i) {
                LmJID *jid;
                
                jid = lm_jid_new (argv[i]);
                
                g_print ("=======( JID[%i] )=======\n", i);
                g_print ("# Name: '%s'\n", jid->name);
                g_print ("# Host: '%s'\n", jid->host);
                if (jid->resource) {
                        g_print ("# Resource: '%s'\n", jid->resource);
                }
                g_print ("-------------------------\n");
                g_print ("# JID: '%s'\n", lm_jid_to_string (jid));
                g_print ("=========================\n\n");
        }
}
