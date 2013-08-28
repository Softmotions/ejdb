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
using System.Text;
using Ejdb.BSON;
using Ejdb.Utils;

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

	/// <summary>
	/// EJDB database native wrapper.
	/// </summary>
	public class EJDB : IDisposable {
		//.//////////////////////////////////////////////////////////////////
		// 						Native open modes
		//.//////////////////////////////////////////////////////////////////
		/// <summary>
		/// Open as a reader.
		/// </summary>
		public const int JBOREADER = 1 << 0;

		/// <summary>
		/// Open as a writer.
		/// </summary>
		public const int JBOWRITER = 1 << 1;

		/// <summary>
		/// Create if db file not exists. 
		/// </summary>
		public const int JBOCREAT = 1 << 2;

		/// <summary>
		/// Truncate db on open.
		/// </summary>
		public const int JBOTRUNC = 1 << 3;

		/// <summary>
		/// Open without locking. 
		/// </summary>
		public const int JBONOLCK = 1 << 4;

		/// <summary>
		/// Lock without blocking.
		/// </summary>
		public const int JBOLCKNB = 1 << 5;

		/// <summary>
		/// Synchronize every transaction with storage.
		/// </summary>
		public const int JBOTSYNC = 1 << 6;

		/// <summary>
		/// The default open mode <c>(JBOWRITER | JBOCREAT)</c>
		/// </summary>
		public const int DEFAULT_OPEN_MODE = (JBOWRITER | JBOCREAT);
		//.//////////////////////////////////////////////////////////////////
		// 				 Native index operations & types (ejdb.h)
		//.//////////////////////////////////////////////////////////////////
		/// <summary>
		/// Drop index.
		/// </summary>
		const int JBIDXDROP = 1 << 0;

		/// <summary>
		/// Drop index for all types.
		/// </summary>
		const int JBIDXDROPALL = 1 << 1;

		/// <summary>
		/// Optimize indexes.
		/// </summary>
		const int JBIDXOP = 1 << 2;

		/// <summary>
		/// Rebuild index.
		/// </summary>
		const int JBIDXREBLD = 1 << 3;

		/// <summary>
		/// Number index.
		/// </summary>
		const int JBIDXNUM = 1 << 4;

		/// <summary>
		/// String index.
		/// </summary>
		const int JBIDXSTR = 1 << 5;

		/// <summary>
		/// Array token index.
		/// </summary>
		const int JBIDXARR = 1 << 6;

		/// <summary>
		/// Case insensitive string index.
		/// </summary>
		const int JBIDXISTR = 1 << 7;

		/// <summary>
		/// The EJDB library version
		/// </summary>
		static string _LIBVERSION;

		/// <summary>
		/// The EJDB library version hex code.
		/// </summary>
		static long _LIBHEXVERSION;

		/// <summary>
		/// Name if EJDB library
		/// </summary>
		#if EJDBDLL
		public const string EJDB_LIB_NAME = "tcejdbdll";
#else
		public const string EJDB_LIB_NAME = "tcejdb";
		#endif
		/// <summary>
		/// Pointer to the native EJDB instance.
		/// </summary>
		IntPtr _db = IntPtr.Zero;

		bool _throwonfail = true;
		//.//////////////////////////////////////////////////////////////////
		//   				Native functions refs
		//.//////////////////////////////////////////////////////////////////

		#region NativeRefs

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbnew", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbnew();

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbdel", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbdel([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbopen", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbopen([In] IntPtr db, [In] IntPtr path, int mode);

		internal static bool _ejdbopen(IntPtr db, string path, int mode) {
			IntPtr pptr = Native.NativeUtf8FromString(path); //UnixMarshal.StringToHeap(path, Encoding.UTF8);
			try {
				return _ejdbopen(db, pptr, mode);
			} finally {
				Marshal.FreeHGlobal(pptr); //UnixMarshal.FreeHeap(pptr);
			}
		}

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbclose", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbclose([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbisopen", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbisopen([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbecode", CallingConvention = CallingConvention.Cdecl)]
		internal static extern int _ejdbecode([In] IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdberrmsg", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdberrmsg(int ecode);

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbgetcoll", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbgetcoll([In] IntPtr db, [In] IntPtr cname);

		internal static IntPtr _ejdbgetcoll(IntPtr db, string cname) {
			IntPtr cptr = Native.NativeUtf8FromString(cname); //UnixMarshal.StringToHeap(cname, Encoding.UTF8);
			try {
				return _ejdbgetcoll(db, cptr);
			} finally {
				Marshal.FreeHGlobal(cptr); //UnixMarshal.FreeHeap(cptr);
			}
		}

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbcreatecoll", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbcreatecoll([In] IntPtr db, [In] IntPtr cname, IntPtr opts);

		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbcreatecoll", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbcreatecoll([In] IntPtr db, [In] IntPtr cname, ref EJDBCollectionOptionsN opts);

		internal static IntPtr _ejdbcreatecoll(IntPtr db, String cname, EJDBCollectionOptionsN? opts) {
			IntPtr cptr = Native.NativeUtf8FromString(cname);//UnixMarshal.StringToHeap(cname, Encoding.UTF8);
			try {
				if (opts == null) {
					return _ejdbcreatecoll(db, cptr, IntPtr.Zero);
				} else {
					EJDBCollectionOptionsN nopts = (EJDBCollectionOptionsN) opts;
					return _ejdbcreatecoll(db, cptr, ref nopts);
				}
			} finally {
				Marshal.FreeHGlobal(cptr); //UnixMarshal.FreeHeap(cptr);
			}
		}
		//EJDB_EXPORT bool ejdbrmcoll(EJDB *jb, const char *colname, bool unlinkfile);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbrmcoll", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbrmcoll([In] IntPtr db, [In] IntPtr cname, bool unlink);

		internal static bool _ejdbrmcoll(IntPtr db, string cname, bool unlink) {
			IntPtr cptr = Native.NativeUtf8FromString(cname);//UnixMarshal.StringToHeap(cname, Encoding.UTF8);
			try {
				return _ejdbrmcoll(db, cptr, unlink);
			} finally {
				Marshal.FreeHGlobal(cptr); //UnixMarshal.FreeHeap(cptr);
			}
		}
		//EJDB_EXPORT bson* ejdbcommand2(EJDB *jb, void *cmdbsondata);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbcommand2", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbcommand([In] IntPtr db, [In] byte[] cmd);
		//EJDB_EXPORT bool ejdbsavebson3(EJCOLL *jcoll, void *bsdata, bson_oid_t *oid, bool merge);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbsavebson3", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbsavebson([In] IntPtr coll, [In] byte[] bsdata, [Out] byte[] oid, [In] bool merge);
		//EJDB_EXPORT bson* ejdbloadbson(EJCOLL *coll, const bson_oid_t *oid);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbloadbson", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbloadbson([In] IntPtr coll, [In] byte[] oid);
		//EJDB_EXPORT const char* bson_data2(const bson *b, int *bsize);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "bson_data2", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _bson_data2([In] IntPtr bsptr, out int size);
		//EJDB_EXPORT void bson_del(bson *b);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "bson_del", CallingConvention = CallingConvention.Cdecl)]
		internal static extern void _bson_del([In] IntPtr bsptr);
		//EJDB_EXPORT bool ejdbrmbson(EJCOLL *coll, bson_oid_t *oid);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbrmbson", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbrmbson([In] IntPtr cptr, [In] byte[] oid);
		//EJDB_EXPORT bool ejdbsyncdb(EJDB *jb)
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbsyncdb", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbsyncdb([In] IntPtr db);
		//EJDB_EXPORT bool ejdbsyncoll(EJDB *jb)
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbsyncoll", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbsyncoll([In] IntPtr coll);
		//EJDB_EXPORT bool ejdbsetindex(EJCOLL *coll, const char *ipath, int flags);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbsetindex", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbsetindex([In] IntPtr coll, [In] IntPtr ipathptr, int flags);
		//EJDB_EXPORT bson* ejdbmeta(EJDB *jb)
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbmeta", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbmeta([In] IntPtr db);
		//EJDB_EXPORT bool ejdbtranbegin(EJCOLL *coll);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbtranbegin", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbtranbegin([In] IntPtr coll);
		//EJDB_EXPORT bool ejdbtrancommit(EJCOLL *coll);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbtrancommit", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbtrancommit([In] IntPtr coll);
		//EJDB_EXPORT bool ejdbtranabort(EJCOLL *coll);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbtranabort", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbtranabort([In] IntPtr coll);
		//EJDB_EXPORT bool ejdbtranstatus(EJCOLL *jcoll, bool *txactive);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbtranstatus", CallingConvention = CallingConvention.Cdecl)]
		internal static extern bool _ejdbtranstatus([In] IntPtr coll, out bool txactive);
		//EJDB_EXPORT const char *ejdbversion();
		[DllImport(EJDB_LIB_NAME, EntryPoint = "ejdbversion", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _ejdbversion();
		//EJDB_EXPORT bson* json2bson(const char *jsonstr);
		[DllImport(EJDB_LIB_NAME, EntryPoint = "json2bson", CallingConvention = CallingConvention.Cdecl)]
		internal static extern IntPtr _json2bson([In] IntPtr jsonstr);

		internal static IntPtr _json2bson(string jsonstr) {
			IntPtr jsonptr = Native.NativeUtf8FromString(jsonstr);
			try {
				return _json2bson(jsonptr);
			} finally {
				Marshal.FreeHGlobal(jsonptr); //UnixMarshal.FreeHeap(jsonptr);
			}
		}

		internal static bool _ejdbsetindex(IntPtr coll, string ipath, int flags) {
			IntPtr ipathptr = Native.NativeUtf8FromString(ipath); //UnixMarshal.StringToHeap(ipath, Encoding.UTF8);
			try {
				return _ejdbsetindex(coll, ipathptr, flags);
			} finally {
				Marshal.FreeHGlobal(ipathptr); //UnixMarshal.FreeHeap(ipathptr);
			}
		}

		#endregion

		/// <summary>
		/// If true <see cref="Ejdb.DB.EJDBException"/> will be thrown in the case of failed operation 
		/// otherwise method will return boolean status flag. Default value: <c>true</c> 
		/// </summary>
		/// <value><c>true</c> if throw exception on fail; otherwise, <c>false</c>.</value>
		public bool ThrowExceptionOnFail {
			get {
				return _throwonfail;
			}
			set {
				_throwonfail = value;
			}
		}

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
				return Native.StringFromNativeUtf8(_ejdberrmsg((int) ecode)); //UnixMarshal.PtrToString(_ejdberrmsg((int) ecode), Encoding.UTF8);
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
		/// Gets info of EJDB database itself and its collections.
		/// </summary>
		/// <value>The DB meta.</value>
		public BSONDocument DBMeta {
			get {
				CheckDisposed(true);
				//internal static extern IntPtr _ejdbmeta([In] IntPtr db);
				IntPtr bsptr = _ejdbmeta(_db);
				if (bsptr == IntPtr.Zero) {
					throw new EJDBException(this);
				}
				try {
					int size;
					IntPtr bsdataptr = _bson_data2(bsptr, out size);
					byte[] bsdata = new byte[size];
					Marshal.Copy(bsdataptr, bsdata, 0, bsdata.Length);
					return new BSONDocument(bsdata);
				} finally {
					_bson_del(bsptr);
				}
			}
		}

		/// <summary>
		/// Gets the EJDB library version.
		/// </summary>
		/// <value>The LIB version.</value>
		public static string LIBVersion {
			get {
				if (_LIBVERSION != null) {
					return _LIBVERSION;
				}
				lock (typeof(EJDB)) {
					if (_LIBVERSION != null) {
						return _LIBVERSION;
					}
					IntPtr vres = _ejdbversion();
					if (vres == IntPtr.Zero) {
						throw new Exception("Unable to get ejdb library version");
					}
					_LIBVERSION = Native.StringFromNativeUtf8(vres); //UnixMarshal.PtrToString(vres, Encoding.UTF8);
				}
				return _LIBVERSION;
			}		
		}

		/// <summary>
		/// Gets the EJDB library hex encoded version.
		/// </summary>
		/// <remarks>
		/// E.g: for the version "1.1.13" return value will be: 0x1113
		/// </remarks>
		/// <value>The lib hex version.</value>
		public static long LibHexVersion {
			get {
				if (_LIBHEXVERSION != 0) {
					return _LIBHEXVERSION;
				}
				lock (typeof(EJDB)) {
					if (_LIBHEXVERSION != 0) {
						return _LIBHEXVERSION;
					}
					_LIBHEXVERSION = Convert.ToInt64("0x" + LIBVersion.Replace(".", ""), 16);
				}
				return _LIBHEXVERSION;
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="Ejdb.DB.EJDB"/> class.
		/// </summary>
		/// <param name="path">The main database file path.</param>
		/// <param name="omode">Open mode.</param>
		public EJDB(string path, int omode = DEFAULT_OPEN_MODE) {
			if (EJDB.LibHexVersion < 0x1113) {
				throw new EJDBException("EJDB library version must be at least '1.1.13' or greater");
			}
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
			bool rv = (cptr != IntPtr.Zero);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Removes collection indetified by <c>cname</c>.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of the collection.</param>
		/// <param name="unlink">If set to <c>true</c> then the collection data file will be removed.</param>
		public bool DropCollection(string cname, bool unlink = false) {
			CheckDisposed();
			bool rv = _ejdbrmcoll(_db, cname, unlink);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Synchronize entire EJDB database and
		/// all of its collections with storage.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		public bool Sync() {
			CheckDisposed();
			//internal static extern bool _ejdbsyncdb([In] IntPtr db);
			bool rv = _ejdbsyncdb(_db);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Synchronize content of a EJDB collection database with the file on device.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		public bool SyncCollection(string cname) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return true;
			}
			//internal static extern bool _ejdbsyncoll([In] IntPtr coll);
			bool rv = _ejdbsyncoll(cptr);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// DROP indexes of all types for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool DropIndexes(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXDROPALL);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// OPTIMIZE indexes of all types for JSON field path.
		/// </summary>
		/// <remarks>
		/// Performs B+ tree index file optimization.
		/// </remarks>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool OptimizeIndexes(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXOP);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Ensure index presence of String type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool EnsureStringIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXSTR);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Rebuild index of String type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool RebuildStringIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXSTR | JBIDXREBLD);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Drop index of String type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool DropStringIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXSTR | JBIDXDROP);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Ensure case insensitive String index for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool EnsureIStringIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXISTR);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Rebuild case insensitive String index for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool RebuildIStringIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXISTR | JBIDXREBLD);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Drop case insensitive String index for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool DropIStringIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXISTR | JBIDXDROP);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Ensure index presence of Number type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool EnsureNumberIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXNUM);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Rebuild index of Number type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool RebuildNumberIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXNUM | JBIDXREBLD);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Drop index of Number type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool DropNumberIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXNUM | JBIDXDROP);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Ensure index presence of Array type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool EnsureArrayIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXARR);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Rebuild index of Array type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool RebuildArrayIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXARR | JBIDXREBLD);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Drop index of Array type for JSON field path.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="ipath">JSON indexed field path</param>
		public bool DropArrayIndex(string cname, string ipath) {
			bool rv = IndexOperation(cname, ipath, JBIDXARR | JBIDXDROP);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Begin transaction for EJDB collection.
		/// </summary>
		/// <returns><c>true</c>, if begin was transactioned, <c>false</c> otherwise.</returns>
		/// <param name="cname">Cname.</param>
		public bool TransactionBegin(string cname) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return true;
			}
			//internal static extern bool _ejdbtranbegin([In] IntPtr coll);
			bool rv = _ejdbtranbegin(cptr);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Commit the transaction.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		public bool TransactionCommit(string cname) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return true;
			}
			//internal static extern bool _ejdbtrancommit([In] IntPtr coll);
			bool rv = _ejdbtrancommit(cptr);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Abort the transaction.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		public bool AbortTransaction(string cname) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return true;
			}
			//internal static extern bool _ejdbtranabort([In] IntPtr coll);
			bool rv = _ejdbtranabort(cptr);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		/// <summary>
		/// Get the transaction status.
		/// </summary>
		/// <returns><c>false</c>, if error occurred.</returns>
		/// <param name="cname">Name of collection.</param>
		/// <param name="active">Out parameter. It <c>true</c> transaction is active.</param>
		public bool TransactionStatus(string cname, out bool active) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				active = false;
				return true;
			}
			bool rv = _ejdbtranstatus(cptr, out active);
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
			}
			return rv;
		}

		public bool Save(string cname, params object[] docs) {
			CheckDisposed();
			IntPtr cptr = _ejdbcreatecoll(_db, cname, null);
			if (cptr == IntPtr.Zero) {
				if (_throwonfail) {
					throw new EJDBException(this);
				} else {
					return false;
				}
			}
			foreach (var doc in docs) {
				if (!Save(cptr, BSONDocument.ValueOf(doc), false)) {
					if (_throwonfail) {
						throw new EJDBException(this);
					} else {
						return false;
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Executes EJDB command.
		/// </summary>
		/// <remarks>
		/// Supported commands:
		///
		/// 1) Exports database collections data. See ejdbexport() method.
		/// 
		/// 	"export" : {
		/// 	"path" : string,                    //Exports database collections data
		/// 	"cnames" : [string array]|null,     //List of collection names to export
		/// 	"mode" : int|null                   //Values: null|`JBJSONEXPORT` See ejdb.h#ejdbexport() method
		/// }
		/// 
		/// Command response:
		/// {
		/// 	"log" : string,        //Diagnostic log about executing this command
		/// 	"error" : string|null, //ejdb error message
		/// 	"errorCode" : int|0,   //ejdb error code
		/// }
		/// 
		/// 2) Imports previously exported collections data into ejdb.
		/// 
		/// 	"import" : {
		/// 	"path" : string                     //The directory path in which data resides
		/// 		"cnames" : [string array]|null,     //List of collection names to import
		/// 		"mode" : int|null                //Values: null|`JBIMPORTUPDATE`|`JBIMPORTREPLACE` See ejdb.h#ejdbimport() method
		/// }
		/// 
		/// Command response:
		/// {
		/// 	"log" : string,        //Diagnostic log about executing this command
		/// 	"error" : string|null, //ejdb error message
		/// 	"errorCode" : int|0,   //ejdb error code
		/// }
		/// </remarks>
		/// <param name="cmd">Command object</param>
		/// <returns>Command response.</returns>
		public BSONDocument Command(BSONDocument cmd) {
			CheckDisposed();
			byte[] cmdata = cmd.ToByteArray();
			//internal static extern IntPtr _ejdbcommand([In] IntPtr db, [In] byte[] cmd);
			IntPtr cmdret = _ejdbcommand(_db, cmdata);
			if (cmdret == IntPtr.Zero) {
				return null;
			}
			byte[] bsdata = BsonPtrIntoByteArray(cmdret);
			if (bsdata.Length == 0) {
				return null;
			}
			BSONIterator it = new BSONIterator(bsdata);
			return it.ToBSONDocument();
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
				if (_throwonfail) {
					throw new EJDBException(this);
				} else {
					return false;
				}
			}
			foreach (var doc in docs) {
				if (!Save(cptr, doc, false)) {
					if (_throwonfail) {
						throw new EJDBException(this);
					} else {
						return false;
					}
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
				if (_throwonfail) {
					throw new EJDBException(this);
				} else {
					return false;
				}
			}
			foreach (var doc in docs) {
				if (!Save(cptr, doc, true)) {
					if (_throwonfail) {
						throw new EJDBException(this);
					} else {
						return false;
					}
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
			if (_throwonfail && !rv) {
				throw new EJDBException(this);
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
		/// Removes stored objects from the collection.
		/// </summary>
		/// <param name="cname">Name of collection.</param>
		/// <param name="oids">Object identifiers.</param>
		public bool Remove(string cname, params BSONOid[] oids) {
			CheckDisposed();
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return true;
			}
			//internal static extern bool _ejdbrmbson([In] IntPtr cptr, [In] byte[] oid);
			foreach (var oid in oids) {
				if (!_ejdbrmbson(cptr, oid.ToBytes())) {
					if (_throwonfail) {
						throw new EJDBException(this);
					} else {
						return false;
					}
				}
			}
			return true;
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

		/// <summary>
		/// Convert JSON string into BSONDocument.
		/// Returns `null` if conversion failed.
		/// </summary>
		/// <returns>The BSONDocument instance on success.</returns>
		/// <param name="json">JSON string</param>
		public BSONDocument Json2Bson(string json) {
			IntPtr bsonret = _json2bson(json);
			if (bsonret == IntPtr.Zero) {
				return null;
			}
			byte[] bsdata = BsonPtrIntoByteArray(bsonret);
			if (bsdata.Length == 0) {
				return null;
			}
			BSONIterator it = new BSONIterator(bsdata);
			return it.ToBSONDocument();
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

		bool IndexOperation(string cname, string ipath, int flags) {
			CheckDisposed(true);
			IntPtr cptr = _ejdbgetcoll(_db, cname);
			if (cptr == IntPtr.Zero) {
				return true;
			}
			//internal static bool _ejdbsetindex(IntPtr coll, string ipath, int flags)
			return _ejdbsetindex(cptr, ipath, flags);
		}

		internal void CheckDisposed(bool checkopen = false) {
			if (_db == IntPtr.Zero) {
				throw new ObjectDisposedException("Database is disposed");
			}
			if (checkopen) {
				if (!IsOpen) {
					throw new ObjectDisposedException("Operation on closed EJDB instance"); 
				}
			}
		}
	}
}

