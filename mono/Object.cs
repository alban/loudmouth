namespace Loudmouth {
  using System;
    public class Object {
	protected IntPtr _obj;

	private Object () {}
	
	protected Object (IntPtr obj) {
	    this._obj = obj;
	}

	internal IntPtr GetObject () {
	    return this._obj;
	}
    }
}
   
