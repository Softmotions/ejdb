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
using System.Text;

namespace Ejdb.IO {

	public class ExtBinaryWriter : BinaryWriter {

		public static Encoding DEFAULT_ENCODING = Encoding.UTF8;
		Encoding _encoding;
		bool _leaveopen;

		public ExtBinaryWriter() {
			_encoding = DEFAULT_ENCODING;
		}

		public ExtBinaryWriter(Stream output) : this(output, DEFAULT_ENCODING, false) {
		}

		public ExtBinaryWriter(Stream output, Encoding encoding, bool leaveopen) : base(output, encoding) {
			_encoding = encoding;
			_leaveopen = leaveopen;
		}

		public ExtBinaryWriter(Stream output, Encoding encoding) : this(output, encoding, false) {
		}

		public ExtBinaryWriter(Stream output, bool leaveopen) : this(output, DEFAULT_ENCODING, leaveopen) {		
		}

		protected override void Dispose(bool disposing) {
			base.Dispose(!_leaveopen);
		}

		public void WriteBSONString(string val) {
			byte[] buf = _encoding.GetBytes(val);
			Write(buf.Length + 1);
			Write(buf);
			Write((byte) 0x00);
		}

		public void WriteCString(string val) {
			if (val.Length > 0) {
				Write(_encoding.GetBytes(val));
			}
			Write((byte) 0x00);
		}
	}
}

