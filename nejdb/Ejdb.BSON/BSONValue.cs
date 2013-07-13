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

namespace Ejdb.BSON {

	/// <summary>
	/// BSON field value.
	/// </summary>
	[Serializable]
	public sealed class BSONValue : IBSONValue, ICloneable {	

		/// <summary>
		/// BSON.Type
		/// </summary>
		public BSONType BSONType { get; internal set; }

		/// <summary>
		/// BSON field key.
		/// </summary>
		public string Key { get; internal set; }

		/// <summary>
		/// Deserialized BSON field value.
		/// </summary>
		public object Value { get; internal set; }

		public BSONValue(BSONType type, string key, object value) {
			this.BSONType = type;
			this.Key = key;
			this.Value = value;
		}

		public BSONValue(BSONType type, string key) : this(type, key, null) {
		}

		public override bool Equals(object obj) {
			if (obj == null) {
				return false;
			}
			if (ReferenceEquals(this, obj)) {
				return true;
			}
			if (obj.GetType() != typeof(BSONValue)) {
				return false;
			}
			BSONValue other = (BSONValue) obj;
			if (BSONType != other.BSONType || Key != other.Key) {
				return false;
			}
			if (Value != null) {
				return Value.Equals(other.Value);
			} else {
				return (Value == other.Value);
			}
		}

		public static bool operator ==(BSONValue v1, BSONValue v2) {
			if (ReferenceEquals(v1, v2)) {
				return true;
			}
			if ((object) v1 == null || (object) v2 == null) {
				return false;
			}
			return v1.Equals(v2);
		}

		public static bool operator !=(BSONValue v1, BSONValue v2) {
			return !(v1 == v2);
		}

		public override int GetHashCode() {
			unchecked {
				return BSONType.GetHashCode() ^ (Value != null ? Value.GetHashCode() : 0);
			}
		}

		public override string ToString() {
			return string.Format("[BSONValue: BSONType={0}, Key={1}, Value={2}]", BSONType, Key, Value);
		}

		public object Clone() {
			return new BSONValue(this.BSONType, this.Key, this.Value);
		}
	}
}

