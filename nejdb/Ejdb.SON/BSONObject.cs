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
using System.Collections.Generic;

namespace Ejdb.SON {

	/// <summary>
	/// BSON document deserialized data wrapper.
	/// </summary>
	[Serializable]
	public class BSONObject : IBSONValue {
		Dictionary<string, BSONValue> _fields;
		readonly List<BSONValue> _fieldslist;

		public BSONObject() {
			this._fields = null;
			this._fieldslist = new List<BSONValue>();
		}

		public virtual BSONType BSONType {
			get {
				return BSONType.OBJECT;
			}
		}

		public BSONObject Add(BSONValue bv) {
			_fieldslist.Add(bv);
			if (_fields != null) {
				_fields.Add(bv.Key, bv);
			}
			return this;
		}

		public BSONValue this[string key] {
			get { 
				return GetBSONValue(key);
			}
			set {
				SetBSONValue(key, value);
			}
		}

		public BSONValue GetBSONValue(string key) {
			CheckFields();
			return _fields[key];
		}

		public void SetBSONValue(string key, BSONValue val) {
			CheckFields();
			var ov = _fields[key];
			if (ov != null) {
				ov.Key = val.Key;
				ov.BSONType = val.BSONType;
				ov.Value = val.Value;
			} else {
				_fieldslist.Add(val);
				_fields[key] = val;
			}
		}

		public object GetObjectValue(string key) {
			CheckFields();
			var bv = _fields[key];
			return bv != null ? bv.Value : null;
		}

		public BSONValue SetObjectValue(string key, object value) {
			throw new NotImplementedException();
		}

		public BSONValue DropValue(string key) {
			var bv = this[key];
			if (bv == null) {
				return bv;
			}
			_fields.Remove(key);
			_fieldslist.RemoveAll(x => x.Key == key);
			return bv;
		}

		protected void CheckFields() {
			if (_fields != null) {
				return;
			}
			_fields = new Dictionary<string, BSONValue>(Math.Max(_fieldslist.Count + 1, 32));
			foreach (var bv in _fieldslist) {
				_fields.Add(bv.Key, bv);
			}
		}
	}
}
