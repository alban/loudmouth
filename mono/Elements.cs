using System;
using System.Runtime.InteropServices;

namespace Loudmouth {

    public class Element  {
	protected IntPtr _obj;
	
	public enum ElementType {
	    MESSAGE,
	    PRESENCE,
	    IQ,
	    STREAM,
	    STREAM_ERROR,
	    UNKNOWN
	}
	
	public enum ElementSubType {
	    NOT_SET = -10,
	    AVAILABLE = -1,
	    NORMAL = 0,
	    CHAT,
	    GROUPCHAT,
	    HEADLINE,
	    UNAVAILABLE,
	    PROBE,
	    SUBSCRIBE,
	    UNSUBSCRIBE,
	    SUBSCRIBED,
	    UNSUBSCRIBED,
	    GET,
	    SET,
	    RESULT,
	    ERROR
	}

	public ElementType Kind {
	    get {
		return (ElementType) lm_message_get_type(this._obj);
	    }
	}
	
	public string this[string data] {
	    get {
		ElementNode node = this.GetNode ();
		return node[data];
	    }
	    set {
		ElementNode node = this.GetNode();
		node[data] = value;
	    }
	}
	
	protected Element (IntPtr obj) 
	{
	    this._obj = obj;
	}

	protected Element (string to, ElementType type) 
	{
	    _obj = lm_message_new (to, type);
	}
	
	protected Element (string to, ElementType type, ElementSubType sub) 
	{
	    _obj = lm_message_new_with_sub_type (to, type, sub);
	}
	
	protected ElementSubType GetElementSubType () 
	{
	    return lm_message_get_sub_type (this._obj);
	}

	public ElementNode GetNode () 
	{
	    return new ElementNode(lm_message_get_node (this._obj));
	}

	public IntPtr GetObject ()
	{
	    return _obj;
	}
	
	public override string ToString () 
	{
	    return "<Element: " + this.Kind + ":" + this.GetElementSubType() + ">";
	}

	public static Element CreateElement (IntPtr obj)
	{
	    switch (lm_message_get_type(obj)) {
		case (int)Element.ElementType.MESSAGE:
		    return new Message(obj);
		case (int)Element.ElementType.PRESENCE:
		    return new Presence(obj);
		case (int)Element.ElementType.IQ:
		    return new IQ(obj);
	    }
	    return null;
	}
 
	[DllImport ("libloudmouth-1.so")]
	    private static extern int lm_message_get_type (IntPtr obj);
	[DllImport ("libloudmouth-1.so")]
	    private static extern IntPtr lm_message_new (string to, ElementType type);
	[DllImport ("libloudmouth-1.so")]
	    private static extern IntPtr lm_message_new_with_sub_type (string to, ElementType type, ElementSubType sub);
	[DllImport ("libloudmouth-1.so")]
	    private static extern ElementSubType lm_message_get_sub_type (IntPtr obj);
	[DllImport ("libloudmouth-1.so")]
	    private static extern IntPtr lm_message_get_node (IntPtr obj);
   }
  
    public class Message : Element {
	public MessageType Type {
	    get {
		return (MessageType) this.GetElementSubType ();
	    }
	}
	public string Body {
	    get {
		ElementNode node = this.GetNode().FindChild("body");
		if (node == null) {
		    return null;
		}
		return node.Value;
	    }
	    set {
		this.GetNode().AddChild("body", value);
	    }
	}
	
	public enum MessageType {
	    NORMAL     = Element.ElementSubType.NOT_SET,
	    CHAT       = Element.ElementSubType.CHAT,
	    GROUPCHAT  = Element.ElementSubType.GROUPCHAT,
	    HEADLINE   = Element.ElementSubType.HEADLINE
	}

	public Message (IntPtr obj) : base (obj) 
	{
	}
	
	public Message (string to) : base (to, Element.ElementType.MESSAGE, Element.ElementSubType.NOT_SET) 
	{
	}
	
	public Message (string to, MessageType type) : base (to, Element.ElementType.MESSAGE, (Element.ElementSubType) type) 
	{
	}
    }

    public class Presence : Element {
	public enum PresenceType {
	    AVAILABLE    = Element.ElementSubType.AVAILABLE,
	    UNAVAILABLE  = Element.ElementSubType.UNAVAILABLE,
	    PROBE        = Element.ElementSubType.PROBE, 
	    SUBSCRIBE    = Element.ElementSubType.SUBSCRIBE, 
	    UNSUBSCRIBE  = Element.ElementSubType.UNSUBSCRIBE,
	    SUBSCRIBED   = Element.ElementSubType.SUBSCRIBED,
	    UNSUBSCRIBED = Element.ElementSubType.UNSUBSCRIBED, 
	}

	public Presence (IntPtr obj) : base (obj) 
	{
	}

	public Presence () : base (null, Element.ElementType.PRESENCE, Element.ElementSubType.AVAILABLE) 
	{
	}
		    
	public Presence (PresenceType type) : base (null, Element.ElementType.PRESENCE, (Element.ElementSubType) type) 
	{
	}
	
	public Presence (string to) : base (to, Element.ElementType.PRESENCE) 
	{
	}
	
	public Presence (string to, PresenceType type) : base (to, Element.ElementType.PRESENCE, (Element.ElementSubType) type) 
	{
	}
    }

    public class IQ : Element {

	public string XMLNs {
	    get {
		ElementNode qNode = this.GetNode().FindChild("query");
		if (qNode != null) {
		    return qNode["xmlns"];
		}
		return "";
	    }
	    set {
		ElementNode qNode = this.GetNode().FindChild("query");
		if (qNode == null) {
		    qNode = this.GetNode().AddChild("query", null);
		}
		
		qNode["xmlns"] = value;
	    }
	}
	
	public enum IQType {
	    GET    = Element.ElementSubType.GET,
	    SET    = Element.ElementSubType.SET,
	    RESULT = Element.ElementSubType.RESULT
	}

	public IQ (IntPtr obj) : base (obj) 
	{
	}

	public IQ () : base (null, Element.ElementType.IQ, Element.ElementSubType.GET) 
	{
	}
	
	public IQ (string to) : base (to, Element.ElementType.IQ, Element.ElementSubType.GET) 
	{
	}
	
	public IQ (string to, IQType type) : base (to, Element.ElementType.IQ, (Element.ElementSubType) type) 
	{
	}

	public IQ (IQType type) : base (null, Element.ElementType.IQ, (Element.ElementSubType) type) 
	{
	}
    }
}
