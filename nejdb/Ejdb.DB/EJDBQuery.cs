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
		//
		#region NativeRefs
		//EJDB_EXPORT void ejdbquerydel(EJQ *q);
		[DllImport(EJDB.EJDB_LIB_NAME, EntryPoint="ejdbquerydel")]
		static extern void _ejdbquerydel([In] IntPtr qptr);
		//EJDB_EXPORT EJQ* ejdbcreatequery2(EJDB *jb, void *qbsdata);
		[DllImport(EJDB.EJDB_LIB_NAME, EntryPoint="ejdbcreatequery2")]
		static extern IntPtr _ejdbcreatequery([In] IntPtr jb, [In] byte[] bsdata);
		//EJDB_EXPORT EJQ* ejdbqueryhints(EJDB *jb, EJQ *q, void *hintsbsdata)
		[DllImport(EJDB.EJDB_LIB_NAME, EntryPoint="ejdbqueryhints")]
		static extern IntPtr _ejdbqueryhints([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
		//EJDB_EXPORT EJQ* ejdbqueryaddor(EJDB *jb, EJQ *q, void *orbsdata)
		[DllImport(EJDB.EJDB_LIB_NAME, EntryPoint="ejdbqueryaddor")]
		static extern IntPtr _ejdbqueryaddor([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
		//EJDB_EXPORT EJQRESULT ejdbqryexecute(EJCOLL *jcoll, const EJQ *q, uint32_t *count, int qflags, TCXSTR *log)
		[DllImport(EJDB.EJDB_LIB_NAME, EntryPoint="ejdbqryexecute")]
		static extern IntPtr _ejdbqryexecute([In] IntPtr jcoll, [In] IntPtr q, out int count, [In] int qflags, [In] IntPtr logxstr);
		#endregion
		internal EJDBQuery(EJDB jb, BSONDocument qdoc) {
			_qptr = _ejdbcreatequery(jb.DBPtr, qdoc.ToByteArray());
			if (_qptr == IntPtr.Zero) {
				throw new EJDBQueryException(jb);
			}
			_jb = jb;
		}

		/// <summary>
		/// Append OR joined restriction to this query.
		/// </summary>
		/// <returns>This query object.</returns>
		/// <param name="doc">Query document.</param>
		public EJDBQuery AppendOR(BSONDocument doc) {
			CheckDisposed();
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
			//static extern IntPtr _ejdbqueryhints([In] IntPtr jb, [In] IntPtr qptr, [In] byte[] bsdata);
			IntPtr qptr = _ejdbqueryhints(_jb.DBPtr, _qptr, hints.ToByteArray());
			if (qptr == IntPtr.Zero) {
				throw new EJDBQueryException(_jb);
			}
			_hints = hints;
			return this;
		}

		public EJDBQCursor Find(string cname, int flags = 0) {
			IntPtr cptr = EJDB._ejdbgetcoll(_jb.DBPtr, cname);
			if (cptr == IntPtr.Zero) {
				return new EJDBQCursor(IntPtr.Zero, 0);
			}
			//static extern IntPtr _ejdbqryexecute([In] IntPtr jcoll, [In] IntPtr q, out int count, [In] int qflags, [In] IntPtr logxstr);
			//todo
			//return new EJDBQCursor();
			return null;
		}

		public BSONIterator FinOne(string cname, int flags = 0) {
			using (EJDBQCursor cur = Find(cname, flags | JBQRYFINDONE_FLAG)) {
				return cur.Next();
			}
		}

		public int Count(string cname, int flags = 0) {
			using (EJDBQCursor cur = Find(cname, flags | JBQRYCOUNT_FLAG)) {
				return cur.Length;
			}
		}

		public int Update(string cname, int flags = 0) {
			using (EJDBQCursor cur = Find(cname, flags | JBQRYCOUNT_FLAG)) {
				return cur.Length;
			}
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

		internal void CheckDisposed() {
			if (_jb == null || _qptr == IntPtr.Zero) {
				throw new ObjectDisposedException("Query object is disposed");
			}
		}
	}
}

