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
using System.Collections.Generic;
using System.Collections;

namespace Ejdb.SON {

	public class BSONIterator : IDisposable, IEnumerable<BSONType>, IEnumerator<BSONType> {

		private readonly BinaryReader _input;
		private BSONType _ctype;
		private int _doclen;
		private bool _first = true;

		public int DocumentLength {
			get { return this._doclen; }
			private set { this._doclen = value; }
		}

		public BSONIterator(byte[] bbuf) : this(new MemoryStream(bbuf)) {
		}

		public BSONIterator(Stream input) {
			this._input = new BinaryReader(input);
			this._doclen = _input.ReadInt32();
			byte bv = _input.ReadByte();
			if (!Enum.IsDefined(typeof(BSONType), (object) bv)) {
				throw new InvalidBSONDataException("Unknown bson type: " + bv);
			}
			this._ctype = (BSONType) bv;
		}

		~BSONIterator() {
			Dispose();
		}

		public BSONType Current {
			get {
				return this._ctype;
			}
		}

		BSONType IEnumerator.Current {
			get {
				return this._ctype;
			}
		}

		IEnumerator IEnumerable.GetEnumerator() { 
			return GetEnumerator();
		}

		public IEnumerator<BSONType> GetEnumerator() {
			return this;
		}

		public void Dispose() {
			if (_input != null) {
				_input.Close();
			}
		}
	}
}

