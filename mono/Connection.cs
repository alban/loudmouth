namespace Loudmouth {
    using System;
    using System.Runtime.InteropServices;
    using System.Collections;

    public class Connection : Object {
	private Hashtable _asyncHandlers;
	private ResultFunction result_func;
	
	public string Server {
	    get {
		return lm_connection_get_server(this._obj);
	    }
	    set {
		lm_connection_set_server(this._obj, value);
	    }
	}

	public uint Port {
	    get {
		return lm_connection_get_port(this._obj);
	    }
	    set {
		lm_connection_set_port(this._obj, value);
	    }
	}

	public bool UseSSL {
	    get {
		return lm_connection_get_use_ssl(this._obj);
	    }
	    set {
		lm_connection_set_use_ssl(this._obj, value);
	    }
	}
	
	public bool IsAuthenticated {
	    get {
		return lm_connection_is_authenticated(this._obj);
	    }
	}

	private delegate int  _ElementHandler (IntPtr h, IntPtr con, IntPtr elementObj, IntPtr ignored);
	private delegate void _ResultFunction (IntPtr con, bool success, IntPtr ptr);
	private delegate void _DisconnectFunction (IntPtr con, int reason, IntPtr ignored);
	
	[DllImport ("libloudmouth.so")]
	    private static extern int lm_message_get_type (IntPtr obj);

	private int HandleElement (IntPtr h, IntPtr con, IntPtr elementObj, IntPtr ignored) {
	    switch (lm_message_get_type(elementObj)) {
		case (int)Element.ElementType.MESSAGE:
		    IncomingMessage(new Message(elementObj));
		    break;
		case (int)Element.ElementType.PRESENCE:
		    IncomingPresence(new Presence(elementObj));
		    break;
		case (int)Element.ElementType.IQ:
		    IncomingIQ(new IQ(elementObj));
		    break;
	    }

	    return 0;
	}

	private int AsyncHandler (IntPtr h, IntPtr con, IntPtr elementObj, IntPtr id) {
	   ElementHandler handler = (ElementHandler) _asyncHandlers[id];
	   if (handler != null) {
	       handler(Element.CreateElement(elementObj));
	   }

	   return 0;
	}
	
	private void _HandleResult (IntPtr con, bool success, IntPtr ptr) {
	    ResultFunction func = this.result_func;
	    this.result_func = null;
	    func(success);
	}

	public delegate void ElementHandler        (Element element);
	public delegate void MessageHandler        (Message message);
	public delegate void PresenceHandler       (Presence presence);
	public delegate void IQHandler             (IQ iq);
	public delegate void ResultFunction        (bool success);
	public delegate void DisconnectionFunction (int reason);

	public event MessageHandler        IncomingMessage;
	public event PresenceHandler       IncomingPresence;
	public event IQHandler             IncomingIQ;
	public event DisconnectionFunction Disconnected;

	[DllImport ("libloudmouth.so")]
	    private static extern IntPtr lm_connection_new (string name);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_open (IntPtr obj, _ResultFunction func, IntPtr ptr, IntPtr ignored2, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_open_and_block (IntPtr obj, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_close (IntPtr obj, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_authenticate (IntPtr obj, string username, string password, string resource, _ResultFunction func, IntPtr ignored, IntPtr ignored2, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_authenticate_and_block (IntPtr obj, string username, string password, string resource, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_is_authenticated (IntPtr obj);
	[DllImport ("libloudmouth.so")]
	    private static extern void lm_connection_set_server (IntPtr obj, string server);
	[DllImport ("libloudmouth.so")]
	    private static extern string lm_connection_get_server (IntPtr obj);
	[DllImport ("libloudmouth.so")]
	    private static extern void lm_connection_set_port (IntPtr obj, uint port);
	[DllImport ("libloudmouth.so")]
	    private static extern uint lm_connection_get_port (IntPtr obj);
	[DllImport ("libloudmouth.so")]
	    private static extern void lm_connection_set_use_ssl (IntPtr obj, bool use_ssl);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_get_use_ssl (IntPtr obj);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_send (IntPtr obj, IntPtr message, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern bool lm_connection_send_with_reply (IntPtr obj, IntPtr message, IntPtr handler, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern IntPtr lm_connection_send_with_reply_and_block (IntPtr obj, IntPtr message, IntPtr errPtr);
	[DllImport ("libloudmouth.so")]
	    private static extern void lm_connection_register_message_handler (IntPtr obj, IntPtr handler, int type, int priority);
	[DllImport ("libloudmouth.so")]
	    private static extern IntPtr lm_message_handler_new (_ElementHandler handler, IntPtr u, IntPtr n);
	public Connection (string server) {
	    this._obj = lm_connection_new(server);
	    this._asyncHandlers = new Hashtable();
	    IntPtr h = lm_message_handler_new(new _ElementHandler (this.HandleElement), IntPtr.Zero, IntPtr.Zero);
	    lm_connection_register_message_handler(this._obj, h, (int)Element.ElementType.MESSAGE, 1);
	    h = lm_message_handler_new(new _ElementHandler(this.HandleElement), IntPtr.Zero, IntPtr.Zero);
	    lm_connection_register_message_handler(this._obj, h, (int)Element.ElementType.PRESENCE, 1);
	    h = lm_message_handler_new(new _ElementHandler(this.HandleElement), IntPtr.Zero, IntPtr.Zero);
	    lm_connection_register_message_handler(this._obj, h, (int)Element.ElementType.IQ, 1);
	}

	public void Open (ResultFunction func) {
	    if (this.result_func != null) {
		// Throw exception?
		Console.WriteLine("Connection::Open: this.result_func != null");
		return;
	    }
	    
	    this.result_func = func;
	    lm_connection_open(this._obj, new _ResultFunction(this._HandleResult), IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
	}

	public bool OpenAndBlock () {
	    return lm_connection_open_and_block(this._obj, IntPtr.Zero);
	}

	public void Close () {
	    lm_connection_close (this._obj, IntPtr.Zero);
	}

	public void Authenticate (Account account, ResultFunction func) {
	    if (this.result_func != null) {
		// Throw exception?
		Console.WriteLine("Connection::Authenticate: this.result_func != null");
		return;
	    }
	    
	    this.result_func = func;
	    lm_connection_authenticate(this._obj, account.username, account.password, account.resource, new _ResultFunction(this._HandleResult), IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
	}

	public bool AuthenticateAndBlock (Account account) {
	    return lm_connection_authenticate_and_block (this._obj, account.username, account.password, account.resource, IntPtr.Zero);
	}
	
	public void Send (Element element) {
	    lm_connection_send (this._obj, element.GetObject (), IntPtr.Zero); 
	}
	
	public void SendWithReply (Element element, ElementHandler handler) {
	    IntPtr ptr = new IntPtr();
	    IntPtr chandler;

	    chandler = lm_message_handler_new(new _ElementHandler(this.AsyncHandler), ptr, IntPtr.Zero);
	    _asyncHandlers.Add(ptr, handler);
	    lm_connection_send_with_reply(_obj, element.GetObject(), chandler, IntPtr.Zero);
	}

	public Element SendWithReplyAndBlock (Element element) {
	    return Element.CreateElement(lm_connection_send_with_reply_and_block(this._obj, element.GetObject(), IntPtr.Zero));
	}
	
	public void SendRaw (string raw) {
	}
    }
}
