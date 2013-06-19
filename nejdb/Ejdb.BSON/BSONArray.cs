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
using System.IO;

namespace Ejdb.BSON {

	[Serializable]
	public class BSONArray : BSONDocument {

		public override BSONType BSONType {
			get {
				return BSONType.ARRAY;
			}
		}

		public object this[int key] {
			get {
				return GetObjectValue(key.ToString());
			}
		}

		public BSONArray() {
		}

		public BSONArray(BSONUndefined[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetUndefined(i);
			}
		}

		public BSONArray(BSONull[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNull(i);
			}
		}

		public BSONArray(ushort[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, (int) arr[i]);
			}
		}

		public BSONArray(uint[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, (long) arr[i]);
			}
		}

		public BSONArray(ulong[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, (long) arr[i]);
			}
		}

		public BSONArray(short[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, (int) arr[i]);
			}
		}

		public BSONArray(string[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetString(i, arr[i]);
			}
		}

		public BSONArray(int[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, arr[i]);
			}
		}

		public BSONArray(long[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, arr[i]);
			}
		}

		public BSONArray(float[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, arr[i]);
			}
		}

		public BSONArray(double[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetNumber(i, arr[i]);
			}
		}

		public BSONArray(bool[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetBool(i, arr[i]);
			}
		}

		public BSONArray(BSONOid[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetOID(i, arr[i]);
			}
		}

		public BSONArray(DateTime[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetDate(i, arr[i]);
			}
		}

		public BSONArray(BSONDocument[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetObject(i, arr[i]);
			}
		}

		public BSONArray(BSONArray[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetArray(i, arr[i]);
			}
		}

		public BSONArray(BSONRegexp[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetRegexp(i, arr[i]);
			}
		}

		public BSONArray(BSONTimestamp[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetTimestamp(i, arr[i]);
			}
		}

		public BSONArray(BSONCodeWScope[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetCodeWScope(i, arr[i]);
			}
		}

		public BSONArray(BSONBinData[] arr) {
			for (var i = 0; i < arr.Length; ++i) {
				SetBinData(i, arr[i]);
			}
		}

		public BSONDocument SetNull(int idx) {
			return base.SetNull(idx.ToString());
		}

		public BSONDocument SetUndefined(int idx) {
			return base.SetUndefined(idx.ToString());
		}

		public BSONDocument SetMaxKey(int idx) {
			return base.SetMaxKey(idx.ToString());
		}

		public BSONDocument SetMinKey(int idx) {
			return base.SetMinKey(idx.ToString());
		}

		public BSONDocument SetOID(int idx, string oid) {
			return base.SetOID(idx.ToString(), oid);
		}

		public BSONDocument SetOID(int idx, BSONOid oid) {
			return base.SetOID(idx.ToString(), oid); 
		}

		public BSONDocument SetBool(int idx, bool val) {
			return base.SetBool(idx.ToString(), val);
		}

		public BSONDocument SetNumber(int idx, int val) {
			return base.SetNumber(idx.ToString(), val);
		}

		public BSONDocument SetNumber(int idx, long val) {
			return base.SetNumber(idx.ToString(), val);
		}

		public BSONDocument SetNumber(int idx, double val) {
			return base.SetNumber(idx.ToString(), val); 
		}

		public BSONDocument SetNumber(int idx, float val) {
			return base.SetNumber(idx.ToString(), val); 
		}

		public BSONDocument SetString(int idx, string val) {
			return base.SetString(idx.ToString(), val); 
		}

		public BSONDocument SetCode(int idx, string val) {
			return base.SetCode(idx.ToString(), val);
		}

		public BSONDocument SetSymbol(int idx, string val) {
			return base.SetSymbol(idx.ToString(), val);
		}

		public BSONDocument SetDate(int idx, DateTime val) {
			return base.SetDate(idx.ToString(), val);
		}

		public BSONDocument SetRegexp(int idx, BSONRegexp val) {
			return base.SetRegexp(idx.ToString(), val);
		}

		public BSONDocument SetBinData(int idx, BSONBinData val) {
			return base.SetBinData(idx.ToString(), val);
		}

		public BSONDocument SetObject(int idx, BSONDocument val) {
			return base.SetDocument(idx.ToString(), val);
		}

		public BSONDocument SetArray(int idx, BSONArray val) {
			return base.SetArray(idx.ToString(), val);
		}

		public BSONDocument SetTimestamp(int idx, BSONTimestamp val) {
			return base.SetTimestamp(idx.ToString(), val);
		}

		public BSONDocument SetCodeWScope(int idx, BSONCodeWScope val) {
			return base.SetCodeWScope(idx.ToString(), val);
		}

		protected override void CheckKey(string key) {
			int idx;
			if (key == null || !int.TryParse(key, out idx) || idx < 0) {
				throw new InvalidBSONDataException(string.Format("Invalid array key: {0}", key));
			}
		}
	}
}

