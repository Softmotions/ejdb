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

namespace Ejdb.DB {

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
		IntPtr _db = (IntPtr) 0x0;
		#region Functions
		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbnew")]
		static extern IntPtr _ejdbnew();

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbdel")]
		static extern IntPtr _ejdbdel(IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbopen", CharSet=CharSet.Auto)]
		static extern bool _ejdbopen(IntPtr db, string path, int mode);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbclose")]
		static extern bool _ejdbclose(IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbisopen")]
		static extern bool _ejdbisopen(IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdbecode")]
		static extern int _ejdbecode(IntPtr db);

		[DllImport(EJDB_LIB_NAME, EntryPoint="ejdberrmsg")]
		static extern string _ejdberrmsg(int ecode);
		#endregion
		/// <summary>
		/// Gets the last DB error code or <c>null</c> if underlying native database object does not exist.
		/// </summary>
		/// <value>The last DB error code.</value>
		public int? LastDBErrorCode {
			get {
				return (_db.ToInt32() != 0x0) ? (int?) _ejdbecode(_db) : null;
			}
		}

		/// <summary>
		/// Gets the last DB error message or <c>null</c> if underlying native database object does not exist.
		/// </summary>
		public string LastDBErrorMsg {
			get {		
				int? ecode = LastDBErrorCode;
				return ecode != null ? _ejdberrmsg((int) ecode) : null;
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
		public EJDB(string path, int omode) {
			bool rv;
			_db = _ejdbnew();
			if (_db.ToInt32() == 0x0) {
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
			if (_db.ToInt32() != 0x0) {
				IntPtr db = _db;
				_db = (IntPtr) 0x0;
				if (db.ToInt32() != 0x0) {
					_ejdbdel(db);
				}
			}
		}
	}
}

