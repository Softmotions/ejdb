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

namespace Ejdb.SON {

	/// <summary>
	/// BSON Timestamp complex value.
	/// </summary>
	[Serializable]
	public class BSONTimestamp : IBSONValue {

		readonly int inc;
		readonly int ts;

		BSONTimestamp() {
		}

		public BSONTimestamp(int inc, int ts) {
			this.inc = inc;
			this.ts = ts;
		}

		public BSONType BSONType {
			get {
				return BSONType.TIMESTAMP;
			}
		}

		public int Inc {
			get {
				return this.inc;
			}
		}

		public int Ts {
			get {
				return this.ts;
			}
		}

		public override string ToString() {
			return string.Format("[BSONTimestamp: inc={0}, ts={1}]", inc, ts);
		}

		public override bool Equals(object obj) {
			if (obj == null)
				return false;
			if (ReferenceEquals(this, obj))
				return true;
			if (obj.GetType() != typeof(BSONTimestamp))
				return false;
			BSONTimestamp other = (BSONTimestamp) obj;
			return inc == other.inc && ts == other.ts;
		}

		public override int GetHashCode() {
			unchecked {
				return inc.GetHashCode() ^ ts.GetHashCode();
			}
		}
	}
}

