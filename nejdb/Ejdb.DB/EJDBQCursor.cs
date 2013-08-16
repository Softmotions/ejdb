// ============================================================================================
//   .NET API for EJDB database library http://ejdb.org
//   Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
//
//   This file is part of EJDB.
//   EJDB is free software; you can redistribute it and/or modify it under the terms of
//   the GNU Lesser General Public License as published by the Free Software Foundation; either
//   version 2.1 of the License or any later version.  EJDB is distributed in the hope
//   that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
//   License for more details.
//   You should have received a copy of the GNU Lesser General Public License along with EJDB;
//   if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
//   Boston, MA 02111-1307 USA.
// ============================================================================================
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using Ejdb.BSON;

namespace Ejdb.DB {

	/// <summary>
	/// Query result set container.
	/// </summary>
	public class EJDBQCursor : IDisposable, IEnumerable<BSONIterator> {
		//optional query execution log buffer
		string _log;
		//current cursor position
		int _pos;
		//cursor length
		int _len;
		//Pointer to the result set list
		IntPtr _qresptr;
		//EJDB_EXPORT void ejdbqresultdispose(EJQRESULT qr);
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbqresultdispose", CallingConvention = CallingConvention.Cdecl)]
		static extern void _ejdbqresultdispose([In] IntPtr qres);
		//const void* ejdbqresultbsondata(EJQRESULT qr, int pos, int *size)
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbqresultbsondata", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _ejdbqresultbsondata([In] IntPtr qres, [In] int pos, out int size);

		/// <summary>
		/// Gets the number of result records stored in this cursor.
		/// </summary>
		public int Length {
			get {
				return _len;
			}
		}

		/// <summary>
		/// Gets optional query execution log.
		/// </summary>
		/// <value>The log.</value>
		public string Log {
			get {
				return _log;
			}
			internal set {
				_log = value;
			}
		}

		internal EJDBQCursor(IntPtr qresptr, int len) {
			_qresptr = qresptr;
			_len = len;
		}

		public BSONIterator this[int idx] {
			get {
				if (_qresptr == IntPtr.Zero || idx >= _len || idx < 0) {
					return null;
				}
				//static extern IntPtr _ejdbqresultbsondata([In] IntPtr qres, [In] int idx, out int size)
				int size;
				IntPtr bsdataptr = _ejdbqresultbsondata(_qresptr, idx, out size);
				if (bsdataptr == IntPtr.Zero) {
					return null;
				}
				byte[] bsdata = new byte[size];
				Marshal.Copy(bsdataptr, bsdata, 0, bsdata.Length);
				return new BSONIterator(bsdata);
			}
		}

		public BSONIterator Next() {
			if (_qresptr == IntPtr.Zero || _pos >= _len) {
				return null;
			}
			return this[_pos++];
		}

		public IEnumerator<BSONIterator> GetEnumerator() {
			BSONIterator it;
			while ((it = Next()) != null) {
				yield return it;
			}
		}

		System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator() {
			return GetEnumerator();
		}

		/// <summary>
		/// Reset cursor position state to its initial value.
		/// </summary>
		public void Reset() {
			_pos = 0;
		}

		public void Dispose() {
			_log = null;
			_pos = 0;
			if (_qresptr != IntPtr.Zero) {
				//static extern void _ejdbqresultdispose([In] IntPtr qres);
				_ejdbqresultdispose(_qresptr);
				_qresptr = IntPtr.Zero;
			}
		}
	}
}

