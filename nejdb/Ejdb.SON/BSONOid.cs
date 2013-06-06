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

	public sealed class BSONOid {

		private byte[] _bytes;

		private BSONOid() {
		}

		public BSONOid(string val) {
			if (val == null) {
				throw new ArgumentNullException("val");
			}
			ParseOIDString(val);
		}

		public BSONOid(byte[] val) {
			if (val == null) {
				throw new ArgumentNullException("val");
			}
			_bytes = new byte[12];
			Array.Copy(val, _bytes, 12);
		}

		private bool IsValidOid(string oid) {
			var i = 0;
			for (; i < oid.Length &&
            	   ((oid[i] >= 0x30 && oid[i] <= 0x39) || (oid[i] >= 0x61 && oid[i] <= 0x66)); 
			     ++i) {
			}
			return (i == 24);
		}

		private void ParseOIDString(string val) {
			if (!IsValidOid(val)) {
				throw new ArgumentException("Invalid oid string");
			}
			var vlen = val.Length;
			_bytes = new byte[vlen / 2];
			for (var i = 0; i < vlen; i += 2) {
				try {
					_bytes[i / 2] = Convert.ToByte(val.Substring(i, 2), 16);
				} catch {
					//failed to convert these 2 chars, they may contain illegal charracters
					_bytes[i / 2] = 0;
				}
			}
		}
	}
}

