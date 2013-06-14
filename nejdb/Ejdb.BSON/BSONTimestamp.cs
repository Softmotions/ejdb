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
	/// BSON Timestamp complex value.
	/// </summary>
	[Serializable]
	public sealed class BSONTimestamp : IBSONValue {

		readonly int _inc;
		readonly int _ts;

		BSONTimestamp() {
		}

		public BSONTimestamp(int inc, int ts) {
			this._inc = inc;
			this._ts = ts;
		}

		public BSONType BSONType {
			get {
				return BSONType.TIMESTAMP;
			}
		}

		public int Inc {
			get {
				return _inc;
			}
		}

		public int Ts {
			get {
				return _ts;
			}
		}

		public override bool Equals(object obj) {
			if (obj == null) {
				return false;
			}
			if (ReferenceEquals(this, obj)) {
				return true;
			}
			if (!(obj is BSONTimestamp)) {
				return false;
			}
			BSONTimestamp other = (BSONTimestamp) obj;
			return (_inc == other._inc && _ts == other._ts);
		}

		public override int GetHashCode() {
			unchecked {
				return (_inc.GetHashCode() ^ _ts.GetHashCode());
			}
		}

		public override string ToString() {
			return string.Format("[BSONTimestamp: inc={0}, ts={1}]", _inc, _ts);
		}
	}
}

