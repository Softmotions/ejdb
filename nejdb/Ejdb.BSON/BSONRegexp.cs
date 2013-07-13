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
	/// BSON Regexp complex value.
	/// </summary>
	[Serializable]
	public sealed class BSONRegexp : IBSONValue {

		readonly string _re;

		readonly string _opts;

		public BSONType BSONType {
			get {
				return BSONType.REGEX;
			}
		}

		public string Re {
			get {
				return this._re;
			}
		}

		public string Opts {
			get {
				return this._opts;
			}
		}

		BSONRegexp() {
		}

		public BSONRegexp(string re) : this(re, "") {
		}

		public BSONRegexp(string re, string opts) {
			this._re = re;
			this._opts = opts;
		}

		public override bool Equals(object obj) {
			if (obj == null) {
				return false;
			}
			if (ReferenceEquals(this, obj)) {
				return true;
			}
			if (!(obj is BSONRegexp)) {
				return false;
			}
			BSONRegexp other = (BSONRegexp) obj;
			return (_re == other._re && _opts == other._opts);
		}

		public override int GetHashCode() {
			unchecked {
				return (_re != null ? _re.GetHashCode() : 0) ^ (_opts != null ? _opts.GetHashCode() : 0);
			}
		}

		public override string ToString() {
			return string.Format("[BSONRegexp: re={0}, opts={1}]", _re, _opts);
		}
	}
}

