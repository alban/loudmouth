# -*- Mode: Python -*-

cdef extern from "Python.h":
    ctypedef struct PyObject
    void Py_INCREF (object)
    void Py_DECREF (object)
    void Py_XINCREF (object)
    void Py_XDECREF (object)
    
cdef extern from "glib/gerror.h":
    ctypedef struct GError:
        int quark
        int domain
        char* message
    
cdef extern from "stdlib.h":
    cdef void *malloc(int size)
    cdef void free(void *ptr)

cdef extern from "glib/gtypes.h":
    ctypedef struct GDestroyNotify
    ctypedef struct gboolean
    
cdef extern from "loudmouth/loudmouth.h":
    ctypedef enum LmMessageType:
        LM_MESSAGE_TYPE_MESSAGE
        LM_MESSAGE_TYPE_PRESENCE
        LM_MESSAGE_TYPE_IQ
        LM_MESSAGE_TYPE_STREAM
        LM_MESSAGE_TYPE_STREAM_ERROR
        LM_MESSAGE_TYPE_UNKNOWN

    ctypedef enum LmMessageSubType:
        LM_MESSAGE_SUB_TYPE_NOT_SET
        LM_MESSAGE_SUB_TYPE_AVAILABLE
        LM_MESSAGE_SUB_TYPE_NORMAL
        LM_MESSAGE_SUB_TYPE_CHAT
        LM_MESSAGE_SUB_TYPE_GROUPCHAT
        LM_MESSAGE_SUB_TYPE_HEADLINE
        LM_MESSAGE_SUB_TYPE_UNAVAILABLE
        LM_MESSAGE_SUB_TYPE_PROBE
        LM_MESSAGE_SUB_TYPE_SUBSCRIBE
        LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE
        LM_MESSAGE_SUB_TYPE_SUBSCRIBED
        LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED
        LM_MESSAGE_SUB_TYPE_GET
        LM_MESSAGE_SUB_TYPE_SET
        LM_MESSAGE_SUB_TYPE_RESULT
        LM_MESSAGE_SUB_TYPE_ERROR

    ctypedef enum LmHandlerResult:
        LM_HANDLER_RESULT_REMOVE_MESSAGE
        LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS

    ctypedef enum LmHandlerPriority:
        LM_HANDLER_PRIORITY_LAST
        LM_HANDLER_PRIORITY_NORMAL
        LM_HANDLER_PRIORITY_FIRST

    ctypedef enum LmDisconnectReason:
        LM_DISCONNECT_REASON_OK
        LM_DISCONNECT_REASON_PING_TIME_OUT
        LM_DISCONNECT_REASON_HUP
        LM_DISCONNECT_REASON_ERROR
        LM_DISCONNECT_REASON_UNKNOWN

    ctypedef enum _LmError:
        LM_ERROR_CONNECTION_NOT_OPEN
        LM_ERROR_CONNECTION_OPEN
        LM_ERROR_AUTH_FAILED

    ctypedef struct LmConnection
    ctypedef struct LmMessage
    ctypedef struct LmMessageNode
    ctypedef struct LmMessageHandler
    ctypedef struct LmMessagePriv
    
    ctypedef struct GError
    ctypedef struct GDestroyNotify
    
    ctypedef void (* LmResultFunction) (LmConnection *connection, int success, void* user_data)
    ctypedef void (* LmDisconnectFunction) (LmConnection *connection, LmDisconnectReason reason, void* user_data)
    
    ctypedef LmHandlerResult (* LmHandleMessageFunction) (LmMessageHandler *handler, LmConnection *connection, LmMessage *message, void* user_data)
    
    LmConnection*        lm_connection_new (char* server)
    int                  lm_connection_open (LmConnection* connection, LmResultFunction function, void* user_data, GDestroyNotify notify, GError** error)
    int                  lm_connection_open_and_block (LmConnection* connection, GError** error)
    int                  lm_connection_close (LmConnection* connection, GError** error)
    int                  lm_connection_authenticate (LmConnection* connection, char* username, char* password, char* resource, LmResultFunction function, void* user_data, GDestroyNotify notify, GError** error)
    int                  lm_connection_authenticate_and_block (LmConnection* connection, char* username, char* password, char* resource, GError** error)
    gboolean             lm_connection_is_open (LmConnection* connection)
    gboolean             lm_connection_is_authenticated (LmConnection* connection)
    char*                lm_connection_get_server (LmConnection* connection)
    void                 lm_connection_set_server (LmConnection* connection, char* server)
    int                  lm_connection_get_port (LmConnection* connection)
    void                 lm_connection_set_port (LmConnection* connection, int port)
    gboolean             lm_connection_get_use_ssl (LmConnection* connection)
    void                 lm_connection_set_use_ssl (LmConnection* connection, int use_ssl)
    int                  lm_connection_send (LmConnection* connection, LmMessage* message, GError** error)
    int                  lm_connection_send_with_reply (LmConnection* connection, LmMessage* message, LmMessageHandler* handler, GError** error)
    LmMessage*           lm_connection_send_with_reply_and_block (LmConnection* connection, LmMessage* message, GError** error)
    void                 lm_connection_register_message_handler (LmConnection* connection, LmMessageHandler* handler, LmMessageType type, LmHandlerPriority priority)
    void                 lm_connection_unregister_message_handler (LmConnection* connection, LmMessageHandler* handler, LmMessageType type)
    void                 lm_connection_set_disconnect_function (LmConnection* connection, LmDisconnectFunction function, void* user_data, GDestroyNotify notify)
    int                  lm_connection_send_raw (LmConnection* connection, char* str, GError** error)
    LmConnection*        lm_connection_ref (LmConnection* connection)
    void                 lm_connection_unref (LmConnection* connection)

    LmMessage*           lm_message_new (char* to, LmMessageType type)
    LmMessage*           lm_message_new_with_sub_type (char* to, LmMessageType type, LmMessageSubType sub_type)
    LmMessageType        lm_message_get_type (LmMessage* message)
    LmMessageSubType     lm_message_get_sub_type (LmMessage* message)
    LmMessageNode*       lm_message_get_node (LmMessage* message)
    LmMessage*           lm_message_ref (LmMessage* message)
    void                 lm_message_unref (LmMessage* message)

    LmMessageHandler*    lm_message_handler_new (LmHandleMessageFunction function, void* user_data, GDestroyNotify notify)
    void                 lm_message_handler_invalidate (LmMessageHandler* handler)
    int                  lm_message_handler_is_valid (LmMessageHandler* handler)
    LmMessageHandler*    lm_message_handler_ref (LmMessageHandler* handler)
    void                 lm_message_handler_unref (LmMessageHandler* handler)

    char*                lm_message_node_get_value (LmMessageNode* node)
    void                 lm_message_node_set_value (LmMessageNode* node, char* value)
    LmMessageNode*       lm_message_node_add_child (LmMessageNode* node, char* name, char* value)
    void                 lm_message_node_set_attribute (LmMessageNode* node, char* name, char* value)
    char*                lm_message_node_get_attribute (LmMessageNode* node, char* name)
    LmMessageNode*       lm_message_node_get_child (LmMessageNode* message_node, char* child_name)
    LmMessageNode*       lm_message_node_find_child (LmMessageNode* message_node, char* child_name)
    int                  lm_message_node_get_raw_mode (LmMessageNode* node)
    void                 lm_message_node_set_raw_mode (LmMessageNode* node, int raw_mode)
    LmMessageNode*       lm_message_node_ref (LmMessageNode* node)
    void                 lm_message_node_unref (LmMessageNode* node)
    char*                lm_message_node_to_string (LmMessageNode* node)

class LmError(Exception):
    pass

# Export all enum values
MESSAGE_TYPE_MESSAGE      = LM_MESSAGE_TYPE_MESSAGE
MESSAGE_TYPE_PRESENCE     = LM_MESSAGE_TYPE_PRESENCE
MESSAGE_TYPE_IQ           = LM_MESSAGE_TYPE_IQ
MESSAGE_TYPE_STREAM       = LM_MESSAGE_TYPE_STREAM
MESSAGE_TYPE_STREAM_ERROR = LM_MESSAGE_TYPE_STREAM_ERROR
MESSAGE_TYPE_UNKNOWN      = LM_MESSAGE_TYPE_UNKNOWN

MESSAGE_SUB_TYPE_NOT_SET      = LM_MESSAGE_SUB_TYPE_NOT_SET
MESSAGE_SUB_TYPE_AVAILABLE    = LM_MESSAGE_SUB_TYPE_AVAILABLE
MESSAGE_SUB_TYPE_NORMAL       = LM_MESSAGE_SUB_TYPE_NORMAL
MESSAGE_SUB_TYPE_CHAT         = LM_MESSAGE_SUB_TYPE_CHAT
MESSAGE_SUB_TYPE_GROUPCHAT    = LM_MESSAGE_SUB_TYPE_GROUPCHAT
MESSAGE_SUB_TYPE_HEADLINE     = LM_MESSAGE_SUB_TYPE_HEADLINE
MESSAGE_SUB_TYPE_UNAVAILABLE  = LM_MESSAGE_SUB_TYPE_UNAVAILABLE
MESSAGE_SUB_TYPE_PROBE        = LM_MESSAGE_SUB_TYPE_PROBE
MESSAGE_SUB_TYPE_SUBSCRIBE    = LM_MESSAGE_SUB_TYPE_SUBSCRIBE
MESSAGE_SUB_TYPE_UNSUBSCRIBE  = LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE
MESSAGE_SUB_TYPE_SUBSCRIBED   = LM_MESSAGE_SUB_TYPE_SUBSCRIBED
MESSAGE_SUB_TYPE_UNSUBSCRIBED = LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED
MESSAGE_SUB_TYPE_GET          = LM_MESSAGE_SUB_TYPE_GET
MESSAGE_SUB_TYPE_SET          = LM_MESSAGE_SUB_TYPE_SET
MESSAGE_SUB_TYPE_RESULT       = LM_MESSAGE_SUB_TYPE_RESULT
MESSAGE_SUB_TYPE_ERROR        = LM_MESSAGE_SUB_TYPE_ERROR

HANDLER_RESULT_REMOVE_MESSAGE      = LM_HANDLER_RESULT_REMOVE_MESSAGE
HANDLER_RESULT_ALLOW_MORE_HANDLERS = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS

HANDLER_PRIORITY_LAST   = LM_HANDLER_PRIORITY_LAST
HANDLER_PRIORITY_NORMAL = LM_HANDLER_PRIORITY_NORMAL
HANDLER_PRIORITY_FIRST  = LM_HANDLER_PRIORITY_FIRST

DISCONNECT_REASON_OK            = LM_DISCONNECT_REASON_OK
DISCONNECT_REASON_PING_TIME_OUT = LM_DISCONNECT_REASON_PING_TIME_OUT
DISCONNECT_REASON_HUP           = LM_DISCONNECT_REASON_HUP
DISCONNECT_REASON_ERROR         = LM_DISCONNECT_REASON_ERROR
DISCONNECT_REASON_UNKNOWN       = LM_DISCONNECT_REASON_UNKNOWN

# XXX: Replace with exceptions
ERROR_CONNECTION_NOT_OPEN = LM_ERROR_CONNECTION_NOT_OPEN
ERROR_CONNECTION_OPEN     = LM_ERROR_CONNECTION_OPEN
ERROR_AUTH_FAILED         = LM_ERROR_AUTH_FAILED

cdef struct DataHelper:
    PyObject *function
    PyObject *extra
    
cdef void cresult_function_handler (LmConnection *connection, int success, void *user_data):
    cdef DataHelper *data
    data = <DataHelper*>user_data
    
    conn = Connection(_create=0)
    conn._set_conn(<object>connection)

    function = <object>data.function
    if data.extra:
        extra = <object>data.extra
        function(conn, success, extra)
    else:
        function(conn, success)    
    
cdef class Connection:
    cdef LmConnection *conn
    
    def __init__(self, server=None, _create=1):

        if not _create:
            return 1
        
        self.conn = lm_connection_new(server)
        lm_connection_ref(self.conn)

    def _set_conn(self, conn):
        self.conn = <LmConnection*>conn
        
    def _get_conn(self):
        return <object>self.conn
    
    def open(self, function, extra=None):
        cdef GError *error
        cdef DataHelper *data
        error = NULL

        data = <DataHelper*>malloc(sizeof(DataHelper))
        data.function = <PyObject*>function
        Py_XINCREF(function)
        if extra:
            data.extra = <PyObject*>extra
            Py_XINCREF(extra)
        else:
            data.extra = NULL

        retval = lm_connection_open (self.conn,
                                     cresult_function_handler,
                                     <void*>data,  #<void*>function,
                                     <GDestroyNotify>NULL,
                                     &error)
        if not retval:
            raise LmError, error.message
    
    def open_and_block(self):
        cdef GError *error
        error = NULL
        
        if not lm_connection_open_and_block(self.conn, &error):
            raise LmError, error.message

    def close(self):
        cdef GError *error
        error = NULL
        
        if not lm_connection_close(self.conn, &error):
            raise LmError, error.message
        
    def authenticate(self, username, password, resource, function, extra=None):
        cdef GError *error
        cdef DataHelper *data
        error = NULL

        data = <DataHelper*>malloc(sizeof(DataHelper))
        data.function = <PyObject*>function
        Py_XINCREF(function)
        if extra:
            data.extra = <PyObject*>extra
            Py_XINCREF(extra)
        else:
            data.extra = NULL

        retval = lm_connection_authenticate (self.conn, username, password, resource,
                                             cresult_function_handler,
                                             <void*>data,
                                             <GDestroyNotify>NULL,
                                             &error)
        if not retval:
            raise LmError, error.message
        
    def authenticate_and_block(self, username, password, resource):
        cdef GError *error
        error = NULL
        
        retval = lm_connection_authenticate_and_block(self.conn, username, password, resource, &error)
        if not retval:
            raise LmError, error.message

    def is_open(self):
        return <int>lm_connection_is_open(self.conn)

    def is_authenticated(self):
        return <int>lm_connection_is_authenticated(self.conn)

    def get_server(self):
        return lm_connection_get_server(self.conn)

    def set_server(self, server):
        lm_connection_set_server(self.conn, server)

    def get_port(self):
        return lm_connection_get_port(self.conn)

    def set_port(self, port):
        lm_connection_set_port(self.conn, port)  

    def get_use_ssl(self):
        return <int>lm_connection_get_use_ssl(self.conn)

    def set_use_ssl(self, boolean):
        lm_connection_set_use_ssl(self.conn, boolean)

    def send(self, message):
        cdef GError *error
        error = NULL

        msg = message._get_msg(message)
        retval = lm_connection_send(self.conn, <LmMessage*>msg, &error)
        if not retval:
            raise LmError, error.message
    
    #int    lm_connection_send_with_reply (LmConnection* connection, LmMessage* message, LmMessageHandler* handler, GError** error)
    #void   lm_connection_register_message_handler (LmConnection* connection, LmMessageHandler* handler, LmMessageType type, LmHandlerPriority priority)
    #void   lm_connection_unregister_message_handler (LmConnection* connection, LmMessageHandler* handler, LmMessageType type)
    #void   lm_connection_set_disconnect_function (LmConnection* connection, LmDisconnectFunction function, void* user_data, GDestroyNotify notify)

    def send_with_reply(self, message, handler, error):
        pass
    
    def send_with_reply_and_block(self, message):
        cdef GError *error
        error = NULL

        print 'send_with_reply_and_block', message
        
        msg = message._get_msg()
        print msg
        retval = <object>lm_connection_send_with_reply_and_block(self.conn,
                                                                 <LmMessage*>msg,
                                                                 &error)
        print 'returned'
        print retval
        if not retval:
            raise LmError, error.message
        
        msg2 = Message(_create=0)
        msg2.set_msg(<object>retval)
        return msg2

    def send_raw(self, str):
        cdef GError *error
        error = NULL

        if not lm_connection_send_raw(self.conn, raw, &error):
            raise LmError, error.message
    
    def register_message_handler(self, handler, type, priority):
        pass

    def unregister_message_handler(self, handler, type, notify):
        pass

cdef class Message:
    cdef LmMessage *msg
    def __init__ (self, to, type, sub_type=None, _create=1):

        if not _create:
            return 1

        if not sub_type:
            self.msg = lm_message_new(to, type)
        else:
            self.msg = lm_message_new_with_sub_type(to, type, subtype)

        lm_message_ref(self.msg)

    def _set_msg(self, msg):
        self.msg = <LmMessage*>msg

    def _get_msg(self):
        return <object>self.msg

    def get_type(self):
        return lm_message_get_type(self.msg)
    
    def get_sub_type(self):
        return lm_message_get_sub_type(self.msg)

    def get_node(self):
        retval = <object>lm_message_get_node(self.msg)
        node = MessageNode(_create=0)
        node._set_node(<object>retval)
        return node
    
cdef LmHandlerResult cmessage_function_handler (LmMessageHandler *handler,
                                                LmConnection     *connection,
                                                LmMessage        *message,
                                                void             *user_data):
    function = <object>user_data
    
    hand = MessageHandler(_create=0)
    hand._set_handler(<object>handler)

    conn = Connection(_create=0)
    conn._set_conn(<object>connection)
    
    msg = Message(_create=0)
    msg._set_msg(<object>message)

    retval = function(hand, conn, msg)

    return retval

cdef class MessageHandler:
    cdef LmMessageHandler* handler
    def __init__(self, function, _create=0):
        if not _create:
            return 0

        self.handler = <LmMessageHandler*>lm_message_handler_new(
            cmessage_function_handler,
            <void*>function,
            <GDestroyNotify>NULL)

        lm_message_handler_ref(self.handler)
        
    def _set_handler(self, handler):
        self.handler = <LmMessageHandler*>handler

    def _get_handler(self, handler):
        return <object>self.handler

    def invalidate(self):
        lm_message_handler_invalidate(self.handler)
        
    def is_valid(self):
        return lm_message_handler_is_valid(self.handler)

cdef class MessageNode:
    cdef LmMessageNode *node
    def __init__(self, _create=1):
        if _create:
            raise TypeError, "MessageNode is an abstract class"

    def _set_node(self, node):
        self.node = <LmMessageNode*>node
        
    def _get_node(self, node):
        return <object>self.node

    def get_value(self):
        return lm_message_node_get_value(self.node)

    def set_value(self, value):
        lm_message_node_set_value(self.node, value)

    def add_child(self, name, value):
        retval = <object>lm_message_node_add_child(self.node, name, value)
        node = MessageNode(_create=0)
        node._set_node(<object>retval)
        return node
            
    def set_attribute(self, name, value):
        lm_message_node_set_attribute(self.node, name, value)

    def get_attribute(self, name):
        return lm_message_node_get_attribute(self.node, name)

    def get_child(self, child_name):
        retval = <object>lm_message_node_get_child(self.node, name)
        node = MessageNode(_create=0)
        node._set_node(<object>retval)
        return node
    
    def find_child(self, child_name):
        retval = <object>lm_message_node_find_child(self.node, name)
        node = MessageNode(_create=0)
        node._set_node(<object>retval)
        return node
    
    def get_raw_mode(self):
        return lm_message_node_get_raw_mode(self.node)
    
    def set_raw_mode(self, raw_mode):
        lm_message_node_set_raw_mode(self.node, raw_mode)

    def to_string(self):
        return lm_message_node_to_string(self.node)

