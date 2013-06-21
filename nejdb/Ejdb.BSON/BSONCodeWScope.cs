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

	[Serializable]
	public sealed class BSONCodeWScope : BSONDocument {

		readonly string _code;

		public override BSONType BSONType {
			get {
				return BSONType.CODEWSCOPE;
			}
		}

		public string Code {
			get {
				return _code;
			}
		}

		public BSONDocument Scope {
			get {
				return this;
			}
		}

		public BSONCodeWScope(string code) {
			this._code = code;
		}

		public override bool Equals(object obj) {
			if (obj == null) {
				return false;
			}
			if (ReferenceEquals(this, obj)) {
				return true;
			}
			if (!(obj is BSONCodeWScope)) {
				return false;
			}
			BSONCodeWScope cw = (BSONCodeWScope) obj;
			if (_code != cw._code) {
				return false;
			}
			return base.Equals(obj);
		}

		public override int GetHashCode() {
			return (_code != null ? _code.GetHashCode() : 0) ^ base.GetHashCode();
		}
	}
}

