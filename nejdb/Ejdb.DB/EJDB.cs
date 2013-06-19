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
using Mono.Unix;
using System.Text;
using Ejdb.BSON;

namespace Ejdb.DB {

	/// <summary>
	/// Corresponds to <c>EJCOLLOPTS</c> in ejdb.h
	/// </summary>
	public struct EJDBCollectionOptionsN {
		[MarshalAs(UnmanagedType.U1)]
		public bool large;

		[MarshalAs(UnmanagedType.U1)]
		public bool compressed;

		public long records;

		public int cachedrecords;
	}

	public class EJDB : IDisposable {
		//Open modes
		public const int JBOREADER = 1 << 0;

		public const int JBOWRITER = 1 << 1;

		public const int JBOCREAT = 1 << 2;

		public const int JBOTRUNC = 1 << 3;

		public const int JBONOLCK = 1 << 4;

		public const int JBOLCKNB = 1 << 5;

		public const int JBOTSYNC = 1 << 6;

		public const int DEFAULT_OPEN_MODE = (JBOWRITER | JBOCREAT);

		public const string EJDB_LIB_NAME = "tcejdb";

		IntPtr _db = IntPtr.Zero;
		//
		#region NativeRefs
		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbnew")]
		internal static extern IntPtr _ejdbnew();

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbdel")]
		internal static extern IntPtr _ejdbdel([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbopen")]
		internal static extern bool _ejdbopen([In] IntPtr db, [In] IntPtr path, int mode);

		internal static bool _ejdbopen(IntPtr db, string path, int mode) {
			IntPtr pptr = UnixMarshal.StringToHeap(path, Encoding.UTF8);
			try {
				return _ejdbopen(db, pptr, mode);
			} finally {
				UnixMarshal.FreeHeap(pptr);
			}
		}

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbclose")]
		internal static extern bool _ejdbclose([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbisopen")]
		internal static extern bool _ejdbisopen([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbecode")]
		internal static extern int _ejdbecode([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdberrmsg")]
		internal static extern IntPtr _ejdberrmsg(int ecode);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbgetcoll")]
		internal static extern IntPtr _ejdbgetcoll([In] IntPtr db, [In] IntPtr cname);

		internal static IntPtr _ejdbgetcoll(IntPtr db, string cname) {
			IntPtr cptr = UnixMarshal.StringToHeap(cname, Encoding.UTF8);
			try {
				return _ejdbgetcoll(db, cptr);
			} finally {
				UnixMarshal.FreeHeap(cptr);
			}
		}

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbcreatecoll")]
		internal static extern IntPtr _ejdbcreatecoll([In] IntPtr db, [In] IntPtr cname, ref EJDBCollectionOptionsN? opts);

		internal static IntPtr _ejdbcreatecoll(IntPtr db, String cname, EJDBCollectionOptionsN? opts) {
			IntPtr cptr = UnixMarshal.StringToHeap(cname, Encoding.UTF8);
			try {
				return _ejdbcreatecoll(db, cptr, ref opts);
			} finally {
				UnixMarshal.FreeHeap(cptr);
			}
		}
		//EJDB_EXPORT bool ejdbsavebson3(EJCOLL *jcoll, void *bsdata, bson_oid_t *oid, bool merge);
		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbsavebson3")]
		internal static extern bool _ejdbsavebson([In] IntPtr coll, [In] byte[] bsdata, [Out] byte[] oid, [In] bool merge);
		//EJDB_EXPORT bson* ejdbloadbson(EJCOLL *coll, const bson_oid_t *oid);
		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbloadbson")]
		internal static extern IntPtr _ejdbloadbson([In] IntPtr coll, [In] byte[] oid);
		//EJDB_EXPORT const char* bson_data2(const bson *b, int *bsize);
		[DllImport(EJDB_LIB_NAME, EntryPoint="bson_data2")]
		internal static extern IntPtr _bson_data2([In] IntPtr bsptr, out int size);
		//EJDB_EXPORT void bson_del(bson *b);
		[DllImport(EJDB_LIB_NAME, EntryPoint="bson_del")]
		internal static extern void _bson_del([In] IntPtr bsptr);
		#endregion
		/// <summary>
		/// Gets the last DB error code or <c>null</c> if underlying native database object does not exist.
		/// </summary>
		/// <value>The last DB error code.</value>
		public int? LastDBErrorCode {
			get {
				return (_db != IntPtr.Zero) ? (int?) _ejdbecode(_db) : null;
			}
		}

		/// <summary>
		/// Gets the last DB error message or <c>null</c> if underlying native database object does not exist.
		/// </summary>
		public string LastDBErrorMsg {
			get {		
				int? ecode = LastDBErrorCode;
				if (ecode == null) {
					return null;
				}
				return UnixMarshal.PtrToString(_ejdberrmsg((int) ecode), Encoding.UTF8);
			}
		}

		/// <summary>
		/// Gets a value indicating whether this EJDB databse is open.
		/// </summary>
		/// <value><c>true</c> if this instance is open; otherwise, <c>false</c>.</value>
		public bool IsOpen {
			get {
				return _ejdbisopen(_db);
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="Ejdb.DB.EJDB"/> class.
		/// </summary>
		/// <param name="path">The main database file path.</param>
		/// <param name="omode">Open mode.</param>
		public EJDB(string path, int omode=DEFAULT_OPEN_MODE) {
			bool rv;
			_db = _ejdbnew();
			if (_db == IntPtr.Zero) {
				throw new EJDBException("Unable to create ejdb instance");
			}
			try {
				rv = _ejdbopen(_db, path, omode); 
			} catch (Exception) {
				Dispose();
				throw;
			}
			if (!rv) {
				throw new EJDBException(this);
			}
		}

		~EJDB() {
			Dispose();
		}

		public void Dispose() {
			if (_db != IntPtr.Zero) {
				IntPtr db = _db;
				_db = IntPtr.Zero;
				if (db != IntPtr.Zero) {
					_ejdbdel(db);
				}
			}
		}

		/// <summary>
		/// Automatically creates new collection if it does't exists.
		/// </summary>
		/// <remarks>
		/// Collection options <c>copts</c> are applied only for newly created collection.
		/// For existing collections <c>copts</c> has no effect.
		/// </remarks>
		/// <returns><c>false</c> error ocurried.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="copts">Collection options.</param>
		public bool EnsureCollection(string cname, EJDBCollectionOptionsN? copts = null) {
			CheckDisposed();
			IntPtr cptr = _ejdbcreatecoll(_db, cname, copts);		
			return (cptr != IntPtr.Zero);
		}

		/// <summary>
		/// Save the BSON document doc into the collection.
		/// </summary>
		/// <param name="cname">Name of collection.</param>
		/// <param name="docs">BSON documents to save.</param>
		/// <returns>True on success.</returns>
		public bool Save(string cname, params BSONDocument[] docs) {
			CheckDisposed();
			IntPtr cptr = _ejdbcreatecoll(_db, cname, null);
			if (cptr == IntPtr.Zero) {
				return false;
			}
			foreach (var doc in docs) {
				if (!Save(cptr, doc, false)) {
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Save the BSON document doc into the collection.
		/// And merge each doc object identified by <c>_id</c> with doc stored in DB.
		/// </summary>
		/// <param name="cname">Name of collection.</param>
		/// <param name="docs">BSON documents to save.</param>
		/// <returns>True on success.</returns>
		public bool SaveMerge(string cname, params BSONDocument[] docs) {
			CheckDisposed();
			IntPtr cptr = _ejdbcreatecoll(_db, cname, null);
			if (cptr == IntPtr.Zero) {
				return false;
			}
			foreach (var doc in docs) {
				if (!Save(cptr, doc, true)) {
					return false;
				}
			}
			return true;
		}

		bool Save(IntPtr cptr, BSONDocument doc, bool merge) {
			bool rv;
			BSONValue bv = doc.GetBSONValue("_id");
			byte[] bsdata = doc.ToByteArray();
			byte[] oiddata = new byte[12];
			//static extern bool _ejdbsavebson([In] IntPtr coll, [In] byte[] bsdata, [Out] byte[] oid, bool merge);
			rv = _ejdbsavebson(cptr, bsdata, oiddata, merge);
			if (rv && bv == null) {
				doc.SetOID("_id", new BSONOid(oiddata));
			}
			return  rv;
		}

		/// <summary>
		/// Loads JSON object identified by OID from the collection.
		/// </summary>
		/// <remarks>
		/// Returns <c>null</c> if object is not found.
		/// </remarks>
		/// <param name="cname">Cname.</param>
		/// <param name="oid">Oid.</param>
		public BSONIterator Load(string cname, BSONOid oid) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return null;
			}
			//static extern IntPtr _ejdbloadbson([In] IntPtr coll, [In] byte[] oid);
			byte[] bsdata = BsonPtrIntoByteArray(_ejdbloadbson(cptr, oid.ToBytes()));
			if (bsdata.Length == 0) {
				return null;
			}
			return new BSONIterator(bsdata);
		}

		/// <summary>
		/// Creates the query.
		/// </summary>
		/// <returns>The query object.</returns>
		/// <param name="qdoc">BSON query spec.</param>
		/// <param name="defaultcollection">Name of the collection used by default.</param>
		public EJDBQuery CreateQuery(object qv = null, string defaultcollection = null) {
			CheckDisposed();
			return new EJDBQuery(this, BSONDocument.ValueOf(qv), defaultcollection);
		}

		public EJDBQuery CreateQueryFor(string defaultcollection) {
			CheckDisposed();
			return new EJDBQuery(this, new BSONDocument(), defaultcollection);
		}
		//.//////////////////////////////////////////////////////////////////
		// 						 Private staff							   //	  
		//.//////////////////////////////////////////////////////////////////
		internal IntPtr DBPtr {
			get {
				CheckDisposed();
				return _db;
			}
		}

		byte[] BsonPtrIntoByteArray(IntPtr bsptr, bool deletebsptr = true) {
			if (bsptr == IntPtr.Zero) {
				return new byte[0];
			}
			int size;
			IntPtr bsdataptr = _bson_data2(bsptr, out size);
			byte[] bsdata = new byte[size];
			Marshal.Copy(bsdataptr, bsdata, 0, bsdata.Length);
			if (deletebsptr) {
				_bson_del(bsptr);
			}
			return bsdata;
		}

		internal void CheckDisposed() {
			if (_db == IntPtr.Zero) {
				throw new ObjectDisposedException("Database is disposed");
			}
		}
	}
}

