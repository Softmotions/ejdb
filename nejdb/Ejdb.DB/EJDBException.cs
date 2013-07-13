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

namespace Ejdb.DB {

	public class EJDBException : Exception {

		public int? Code {
			get;
			private set;
		}

		public EJDBException() {
		}

		public EJDBException(string msg) : base(msg) {
		}

		public EJDBException(int code, string msg) : base(msg) {
			this.Code = code;
		}

		public EJDBException(EJDB db) : base(db.LastDBErrorMsg) {
			this.Code = db.LastDBErrorCode;
		}

		public override string ToString() {
			return string.Format("[EJDBException: Code={0}, Msg={1}]", Code, Message);
		}
	}
}

