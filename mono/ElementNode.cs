using System;
using System.Runtime.InteropServices;

namespace Loudmouth {
    
    public class ElementNode {
	private IntPtr _obj;

	public string Value {
	    get {
		return lm_message_node_get_value (this._obj);
	    } 
	    set {
		lm_message_node_set_value (this._obj, value);
	    }
	}

	[DllImport ("libloudmouth-1.so")]
	    private static extern string lm_message_node_get_value (IntPtr obj);
	[DllImport ("libloudmouth-1.so")]
	    private static extern void lm_message_node_set_value (IntPtr obj, string value);
	[DllImport ("libloudmouth-1.so")]
	    private static extern IntPtr lm_message_node_add_child (IntPtr obj, string name, string value);
	[DllImport ("libloudmouth-1.so")]
	    private static extern void lm_message_node_set_attribute (IntPtr obj, string name, string value);
	[DllImport ("libloudmouth-1.so")]
	    private static extern string lm_message_node_get_attribute (IntPtr obj, string name);
	[DllImport ("libloudmouth-1.so")]
	    private static extern IntPtr lm_message_node_get_child (IntPtr obj, string name);
	
	[DllImport ("libloudmouth-1.so")]
	    private static extern IntPtr lm_message_node_find_child (IntPtr obj, string name);

	public ElementNode (IntPtr obj) {
	    this._obj = obj;
	}
	
	public ElementNode AddChild (string name, string value) {
	    return new ElementNode(lm_message_node_add_child(this._obj, name, value));
	}

	public ElementNode GetChild (string name) {
	    IntPtr cChild = lm_message_node_get_child(this._obj, name);
	    if (cChild.ToInt32() == 0) {
		return null;
	    }
	    return new ElementNode(cChild);
	}

	public ElementNode FindChild (string name) {
	    IntPtr cChild = lm_message_node_find_child(this._obj, name);
	    if (cChild.ToInt32() == 0) {
		return null;
	    }
	    return new ElementNode(cChild);
	}

	public string this[string data] {
	    get {
		return lm_message_node_get_attribute(this._obj, data);
	    }
	    set {
		Console.WriteLine("Setting {0} = {1}: {2}", data, value, this._obj);
		lm_message_node_set_attribute (this._obj, data, value);
	    }
	}

	public override string ToString () {
	    return "Loudmouth.ElementNode";
	}
    }
}
