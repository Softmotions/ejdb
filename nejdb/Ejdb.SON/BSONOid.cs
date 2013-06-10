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
using System.Text;
using System.IO;

namespace Ejdb.SON {

	[Serializable]
	public sealed class BSONOid : IComparable<BSONOid>, IBSONValue {

		internal byte[] Bytes;
		string _cachedString;

		public BSONType BSONType {
			get {
				return BSONType.OID;
			}
		}

		BSONOid() {
		}

		public BSONOid(string val) {
			ParseOIDString(val);
		}

		public BSONOid(byte[] val) {
			Bytes = new byte[12];
			Array.Copy(val, Bytes, 12);
		}

		public BSONOid(BinaryReader reader) {
			Bytes = reader.ReadBytes(12);
		}

		bool IsValidOid(string oid) {
			var i = 0;
			for (; i < oid.Length &&
            	   ((oid[i] >= 0x30 && oid[i] <= 0x39) || (oid[i] >= 0x61 && oid[i] <= 0x66)); 
			     ++i) {
			}
			return (i == 24);
		}

		void ParseOIDString(string val) {
			if (!IsValidOid(val)) {
				throw new ArgumentException("Invalid oid string");
			}
			var vlen = val.Length;
			Bytes = new byte[vlen / 2];
			for (var i = 0; i < vlen; i += 2) {
				Bytes[i / 2] = Convert.ToByte(val.Substring(i, 2), 16);
			}
		}

		public int CompareTo(BSONOid other) {
			if (ReferenceEquals(other, null)) {
				return 1;
			}
			var obytes = other.Bytes;
			for (var x = 0; x < Bytes.Length; x++) {
				if (Bytes[x] < obytes[x]) {
					return -1;
				}
				if (Bytes[x] > obytes[x]) {
					return 1;
				}			
			}
			return 0;
		}

		public override string ToString() {
			if (_cachedString == null) {
				_cachedString = BitConverter.ToString(Bytes).Replace("-", "").ToLower();
			}
			return _cachedString;
		}

		public override bool Equals(object obj) {
			if (obj is BSONOid) {
				return (CompareTo((BSONOid) obj) == 0);
			}
			return false;
		}

		public override int GetHashCode() {
			return ToString().GetHashCode();
		}

		public static bool operator ==(BSONOid a, BSONOid b) {
			if (ReferenceEquals(a, b)) {
				return true;
			}
			if (a == null || b == null) {
				return false;
			}
			return a.Equals(b);
		}

		public static bool operator !=(BSONOid a, BSONOid b) {
			return !(a == b);
		}

		public static bool operator >(BSONOid a, BSONOid b) {
			return a.CompareTo(b) > 0;
		}

		public static bool operator <(BSONOid a, BSONOid b) {
			return a.CompareTo(b) < 0;
		}

		public static explicit operator BSONOid(string val) {
			return new BSONOid(val);
		}
	}
}
