import time
import gtk
import loudmouth

lm = loudmouth

conn = lm.Connection("jabber.org")
conn.set_port(5222)

def result_cb(conn, success):
    if success:
        print "Connection successful"
    else:
        print "Connection failed"
        gtk.mainquit()

def auth_cb(conn, success):
    if success:
        print 'Auth successful'
    else:
        print "Auth failed"
    gtk.mainquit()
    
conn.open(result_cb)
conn.authenticate('jdahlin', 'amiga', 'gossip', auth_cb)
msg = lm.Message("richard@imendio.com", lm.MESSAGE_TYPE_MESSAGE)
conn.send_with_reply_and_block(msg)
gtk.main()
