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
using Ejdb.BSON;
using System.Runtime.InteropServices;
using System.Text;
using Ejdb.Utils;

namespace Ejdb.DB {

	/// <summary>
	/// EJDB query.
	/// </summary>
	public class EJDBQuery : IDisposable {

		//Query search mode flags in ejdbqryexecute()
		/// <summary>
		/// Query only count(*)
		/// </summary>
		public const int JBQRYCOUNT_FLAG = 1;

		/// <summary>
		/// Fetch first record only.
		/// </summary>
		public const int JBQRYFINDONE_FLAG = 1 << 1;

		/// <summary>
		/// Explain query execution and 
		/// store query execution log into <see cref="Ejdb.DB.EJDBQCursor#Log"/>
		/// </summary>
		public const int EXPLAIN_FLAG = 1 << 16;

		/// <summary>
		/// EJDB query object <c>EJQ</c> pointer.
		/// </summary>
		IntPtr _qptr = IntPtr.Zero;

		/// <summary>
		/// Database reference.
		/// </summary>
		EJDB _jb;

		/// <summary>
		/// Last used query hints document.
		/// </summary>
		BSONDocument _hints;

		/// <summary>
		/// Name of the collection used by default.
		/// </summary>
		string _defaultcollection;

		/// <summary>
		/// If true query hints need to be saved in the native query object.
		/// </summary>
		bool _dutyhints;
		//
		#region NativeRefs
		//EJDB_EXPORT void ejdbquerydel(EJQ *q);
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbquerydel", CallingConvention = CallingConvention.Cdecl)]
		static extern void _ejdbquerydel([In] IntPtr qptr);
		//EJDB_EXPORT EJQ* ejdbcreatequery2(EJDB *jb, void *qbsdata);
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbcreatequery2", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _ejdbcreatequery([In] IntPtr jb, [In] byte[] bsdata);
		//EJDB_EXPORT EJQ* ejdbqueryhints(EJDB *jb, EJQ *q, void *hintsbsdata)
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbqueryhints", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _ejdbqueryhints([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
		//EJDB_EXPORT EJQ* ejdbqueryaddor(EJDB *jb, EJQ *q, void *orbsdata)
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbqueryaddor", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _ejdbqueryaddor([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
		//EJDB_EXPORT EJQRESULT ejdbqryexecute(EJCOLL *jcoll, const EJQ *q, uint32_t *count, int qflags, TCXSTR *log)
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "ejdbqryexecute", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _ejdbqryexecute([In] IntPtr jcoll, [In] IntPtr q, out int count, [In] int qflags, [In] IntPtr logxstr);
		//EJDB_EXPORT TCXSTR *tcxstrnew(void)
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "tcxstrnew", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _tcxstrnew();
		//EJDB_EXPORT void tcxstrdel(TCXSTR *xstr);
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "tcxstrdel", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _tcxstrdel([In] IntPtr strptr);
		//EJDB_EXPORT int tcxstrsize(const TCXSTR *xstr);
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "tcxstrsize", CallingConvention = CallingConvention.Cdecl)]
		static extern int _tcxstrsize([In] IntPtr strptr);
		//EJDB_EXPORT int tcxstrptr(const TCXSTR *xstr);
        [DllImport(EJDB.EJDB_LIB_NAME, EntryPoint = "tcxstrptr", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr _tcxstrptr([In] IntPtr strptr);
		#endregion
		/// <summary>
		/// Name of default collection used by this query.
		/// </summary>
		/// <value>Name of EJDB collection.</value>
		public string DefaultCollection {
			get {
				return _defaultcollection;
			}
			set {
				_defaultcollection = value;
			}
		}

		internal EJDBQuery(EJDB jb, BSONDocument qdoc, string defaultcollection = null) {
			_qptr = _ejdbcreatequery(jb.DBPtr, qdoc.ToByteArray());
			if (_qptr == IntPtr.Zero) {
				throw new EJDBQueryException(jb);
			}
			_jb = jb;
			_defaultcollection = defaultcollection;
		}

		/// <summary>
		/// Append OR joined restriction to this query.
		/// </summary>
		/// <returns>This query object.</returns>
		/// <param name="doc">Query document.</param>
		public EJDBQuery AddOR(object docobj) {
			CheckDisposed();
			BSONDocument doc = BSONDocument.ValueOf(docobj);
			//static extern IntPtr _ejdbqueryaddor([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
			IntPtr qptr = _ejdbqueryaddor(_jb.DBPtr, _qptr, doc.ToByteArray());
			if (qptr == IntPtr.Zero) {
				throw new EJDBQueryException(_jb);
			}
			return  this;
		}

		/// <summary>
		/// Sets the query hints. 
		/// </summary>
		/// <remarks>
		/// Replaces previous hints associated with this query.
		/// </remarks>
		/// <returns>This query object.</returns>
		/// <param name="hints">Hints document.</param>
		public EJDBQuery SetHints(BSONDocument hints) {
			CheckDisposed();
			//0F-00-00-00-10-24-6D-61-78-00-0A-00-00-00-00
			//static extern IntPtr _ejdbqueryhints([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
			IntPtr qptr = _ejdbqueryhints(_jb.DBPtr, _qptr, hints.ToByteArray());
			if (qptr == IntPtr.Zero) {
				throw new EJDBQueryException(_jb);
			}
			_hints = hints;
			return this;
		}

		public EJDBQCursor Find(string cname = null, int qflags = 0) {
			CheckDisposed();
			if (cname == null) {
				cname = _defaultcollection;
			}
			if (cname == null) {
				throw new ArgumentException("Collection name must be provided");
			}
			IntPtr cptr = EJDB._ejdbgetcoll(_jb.DBPtr, cname);
			if (cptr == IntPtr.Zero) {
				return new EJDBQCursor(IntPtr.Zero, 0);
			}
			if (_dutyhints) {
				SetHints(_hints);
				_dutyhints = false;
			}
			int count;
			IntPtr logsptr = IntPtr.Zero;
			if ((qflags & EXPLAIN_FLAG) != 0) {
				//static extern IntPtr _tcxstrnew();
				logsptr = _tcxstrnew(); //Create dynamic query execution log buffer
			}
			EJDBQCursor cur = null;
			try {
				//static extern IntPtr _ejdbqryexecute([In] IntPtr jcoll, [In] IntPtr q, out int count, [In] int qflags, [In] IntPtr logxstr);
				IntPtr qresptr = _ejdbqryexecute(cptr, _qptr, out count, qflags, logsptr);
				cur = new EJDBQCursor(qresptr, count);
			} finally {
				if (logsptr != IntPtr.Zero) {
					try {
						if (cur != null) {
							//static extern IntPtr _tcxstrptr([In] IntPtr strptr);
							IntPtr sbptr = _tcxstrptr(logsptr);
							cur.Log = Native.StringFromNativeUtf8(sbptr); //UnixMarshal.PtrToString(sbptr, Encoding.UTF8);
						}
					} finally {
						//static extern IntPtr _tcxstrdel([In] IntPtr strptr);
						_tcxstrdel(logsptr);
					}
				}
			}
			int ecode = _jb.LastDBErrorCode ?? 0;
			if (ecode != 0) {
				cur.Dispose();
				throw new EJDBException(_jb);
			}
			return cur;
		}

		public BSONIterator FinOne(string cname = null, int qflags = 0) {
			using (EJDBQCursor cur = Find(cname, qflags | JBQRYFINDONE_FLAG)) {
				return cur.Next();
			}
		}

		public int Count(string cname = null, int qflags = 0) {
			using (EJDBQCursor cur = Find(cname, qflags | JBQRYCOUNT_FLAG)) {
				return cur.Length;
			}
		}

		public int Update(string cname = null, int qflags = 0) {
			using (EJDBQCursor cur = Find(cname, qflags | JBQRYCOUNT_FLAG)) {
				return cur.Length;
			}
		}
		//.//////////////////////////////////////////////////////////////////
		// 						 Hints helpers							   //  	
		//.//////////////////////////////////////////////////////////////////	
		public EJDBQuery Max(int max) {
			if (max < 0) {
				throw new ArgumentException("Max limit cannot be negative");
			}
			if (_hints == null) {
				_hints = new BSONDocument();
			}
			_hints["$max"] = max;
			_dutyhints = true;
			return this;
		}

		public EJDBQuery Skip(int skip) {
			if (skip < 0) {
				throw new ArgumentException("Skip value cannot be negative");
			}
			if (_hints == null) {
				_hints = new BSONDocument();
			}
			_hints["$skip"] = skip;
			_dutyhints = true;
			return this;
		}

		public EJDBQuery OrderBy(string field, bool asc = true) {
			if (_hints == null) {
				_hints = new BSONDocument();
			}
			BSONDocument oby = _hints["$orderby"] as BSONDocument;
			if (oby == null) {
				oby = new BSONDocument();
				_hints["$orderby"] = oby;
			}
			oby[field] = (asc) ? 1 : -1;
			_dutyhints = true;
			return this;
		}

		public EJDBQuery IncludeFields(params string[] fields) {
			return IncExFields(fields, 1);
		}

		public EJDBQuery ExcludeFields(params string[] fields) {
			return IncExFields(fields, 0);
		}

		public void Dispose() {
			if (_qptr != IntPtr.Zero) {
				//static extern void _ejdbquerydel([In] IntPtr qptr);
				_ejdbquerydel(_qptr);
				_qptr = IntPtr.Zero;
			}
			if (_jb != null) {
				_jb = null;
			}
			_hints = null;
		}

		public EJDBQuery SetDefaultCollection(string cname) {
			_defaultcollection = cname;
			return this;
		}
		//.//////////////////////////////////////////////////////////////////
		// 						Privates								   //									  
		//.//////////////////////////////////////////////////////////////////
		EJDBQuery IncExFields(string[] fields, int inc) {
			if (_hints == null) {
				_hints = new BSONDocument();
			}
			BSONDocument fdoc = _hints["$fields"] as BSONDocument;
			if (fdoc == null) {
				fdoc = new BSONDocument();
				_hints["$fields"] = fdoc;
			}
			foreach (var fn in fields) {
				fdoc[fn] = inc;
			}
			_dutyhints = true;
			return this;
		}

		internal void CheckDisposed() {
			if (_jb == null || _qptr == IntPtr.Zero) {
				throw new ObjectDisposedException("Query object is disposed");
			}
		}
	}
}

