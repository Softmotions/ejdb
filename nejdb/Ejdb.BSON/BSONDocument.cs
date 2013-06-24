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
using System.IO;
using System.Text;
using System.Diagnostics;
using Ejdb.IO;
using Ejdb.BSON;
using Ejdb.Utils;
using System.Reflection;
using System.Linq;

namespace Ejdb.BSON {

	/// <summary>
	/// BSON document deserialized data wrapper.
	/// </summary>
	[Serializable]
	public class BSONDocument : IBSONValue, IEnumerable<BSONValue>, ICloneable {
		static Dictionary<Type, Action<BSONDocument, string, object>> TYPE_SETTERS = 
		new Dictionary<Type, Action<BSONDocument, string, object>> {
			{typeof(bool), (d, k, v) => d.SetBool(k, (bool) v)},
			{typeof(bool[]), (d, k, v) => d.SetArray(k, new BSONArray((bool[]) v))},
			{typeof(byte), (d, k, v) => d.SetNumber(k, (int) v)},
			{typeof(sbyte), (d, k, v) => d.SetNumber(k, (int) v)},
			{typeof(ushort), (d, k, v) => d.SetNumber(k, (int) v)},
			{typeof(ushort[]), (d, k, v) => d.SetArray(k, new BSONArray((ushort[]) v))},
			{typeof(short), (d, k, v) => d.SetNumber(k, (int) v)},
			{typeof(short[]), (d, k, v) => d.SetArray(k, new BSONArray((short[]) v))},
			{typeof(uint), (d, k, v) => d.SetNumber(k, (int) v)},
			{typeof(uint[]), (d, k, v) => d.SetArray(k, new BSONArray((uint[]) v))},
			{typeof(int), (d, k, v) => d.SetNumber(k, (int) v)},
			{typeof(int[]), (d, k, v) => d.SetArray(k, new BSONArray((int[]) v))},
			{typeof(ulong), (d, k, v) => d.SetNumber(k, (long) v)},
			{typeof(ulong[]), (d, k, v) => d.SetArray(k, new BSONArray((ulong[]) v))},
			{typeof(long), (d, k, v) => d.SetNumber(k, (long) v)},
			{typeof(long[]), (d, k, v) => d.SetArray(k, new BSONArray((long[]) v))},
			{typeof(float), (d, k, v) => d.SetNumber(k, (float) v)},
			{typeof(float[]), (d, k, v) => d.SetArray(k, new BSONArray((float[]) v))},
			{typeof(double), (d, k, v) => d.SetNumber(k, (double) v)},
			{typeof(double[]), (d, k, v) => d.SetArray(k, new BSONArray((double[]) v))},
			{typeof(char), (d, k, v) => d.SetString(k, v.ToString())},
			{typeof(string), (d, k, v) => d.SetString(k, (string) v)},
			{typeof(string[]), (d, k, v) => d.SetArray(k, new BSONArray((string[]) v))},
			{typeof(BSONOid), (d, k, v) => d.SetOID(k, (BSONOid) v)},
			{typeof(BSONOid[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONOid[]) v))},
			{typeof(BSONRegexp), (d, k, v) => d.SetRegexp(k, (BSONRegexp) v)},
			{typeof(BSONRegexp[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONRegexp[]) v))},
			{typeof(BSONValue), (d, k, v) => d.SetBSONValue((BSONValue) v)},
			{typeof(BSONTimestamp), (d, k, v) => d.SetTimestamp(k, (BSONTimestamp) v)},
			{typeof(BSONTimestamp[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONTimestamp[]) v))},
			{typeof(BSONCodeWScope), (d, k, v) => d.SetCodeWScope(k, (BSONCodeWScope) v)},
			{typeof(BSONCodeWScope[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONCodeWScope[]) v))},
			{typeof(BSONBinData), (d, k, v) => d.SetBinData(k, (BSONBinData) v)},
			{typeof(BSONBinData[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONBinData[]) v))},
			{typeof(BSONDocument), (d, k, v) => d.SetDocument(k, (BSONDocument) v)},
			{typeof(BSONDocument[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONDocument[]) v))},
			{typeof(BSONArray), (d, k, v) => d.SetArray(k, (BSONArray) v)},
			{typeof(BSONArray[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONArray[]) v))},
			{typeof(DateTime), (d, k, v) => d.SetDate(k, (DateTime) v)},
			{typeof(DateTime[]), (d, k, v) => d.SetArray(k, new BSONArray((DateTime[]) v))},
			{typeof(BSONUndefined), (d, k, v) => d.SetUndefined(k)},
			{typeof(BSONUndefined[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONUndefined[]) v))},
			{typeof(BSONull), (d, k, v) => d.SetNull(k)},
			{typeof(BSONull[]), (d, k, v) => d.SetArray(k, new BSONArray((BSONull[]) v))}
		};

		readonly List<BSONValue> _fieldslist;

		[NonSerializedAttribute]
		Dictionary<string, BSONValue> _fields;

		[NonSerializedAttribute]
		int? _cachedhash;

		/// <summary>
		/// BSON Type this document. 
		/// </summary>
		/// <remarks>
		/// Type can be either <see cref="Ejdb.BSON.BSONType.OBJECT"/> or <see cref="Ejdb.BSON.BSONType.ARRAY"/>
		/// </remarks>
		/// <value>The type of the BSON.</value>
		public virtual BSONType BSONType {
			get { 
				return BSONType.OBJECT;
			}
		}

		/// <summary>
		/// Gets the document keys.
		/// </summary>
		public ICollection<string> Keys {
			get {
				CheckFields();
				return _fields.Keys;
			}
		}

		/// <summary>
		/// Gets count of document keys.
		/// </summary>
		public int KeysCount {
			get {
				return _fieldslist.Count;
			}
		}

		public BSONDocument() {
			this._fields = null;
			this._fieldslist = new List<BSONValue>();
		}

		public BSONDocument(BSONIterator it) : this() {
			while (it.Next() != BSONType.EOO) {
				Add(it.FetchCurrentValue());
			}
		}

		public BSONDocument(BSONIterator it, string[] fields) : this() {
			Array.Sort(fields);
			BSONType bt;
			int ind = -1;
			int nfc = 0;
			foreach (string f in fields) {
				if (f != null) {
					nfc++;
				}
			}
			while ((bt = it.Next()) != BSONType.EOO) {
				if (nfc < 1) {
					continue;
				}
				string kk = it.CurrentKey;
				if ((ind = Array.IndexOf(fields, kk)) != -1) {
					Add(it.FetchCurrentValue());
					fields[ind] = null;
					nfc--;
				} else if (bt == BSONType.OBJECT || bt == BSONType.ARRAY) {
					string[] narr = null;
					for (var i = 0; i < fields.Length; ++i) {
						var f = fields[i];
						if (f == null) {
							continue;
						}
						if (f.IndexOf(kk, StringComparison.Ordinal) == 0 && 
							f.Length > kk.Length + 1 && 
							f[kk.Length] == '.') {
							if (narr == null) {
								narr = new string[fields.Length];
							}
							narr[i] = f.Substring(kk.Length + 1);
							fields[i] = null;
							nfc--;
						}
					}
					if (narr != null) {
						BSONIterator nit = new BSONIterator(it);
						BSONDocument ndoc = new BSONDocument(nit, narr);
						if (ndoc.KeysCount > 0) {
							Add(new BSONValue(bt, kk, ndoc));
						}
					}
				}
			}
			it.Dispose();
		}

		public BSONDocument(byte[] bsdata) : this() {
			using (BSONIterator it = new BSONIterator(bsdata)) {
				while (it.Next() != BSONType.EOO) {
					Add(it.FetchCurrentValue());
				}
			}
		}

		public BSONDocument(Stream bstream) : this() {
			using (BSONIterator it = new BSONIterator(bstream)) {
				while (it.Next() != BSONType.EOO) {
					Add(it.FetchCurrentValue());
				}
			}
		}

		public BSONDocument(BSONDocument doc) : this() {
			foreach (var bv in doc) {
				Add((BSONValue) bv.Clone());
			}
		}

		public IEnumerator<BSONValue> GetEnumerator() {
			return _fieldslist.GetEnumerator();
		}

		System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator() {
			return GetEnumerator();
		}

		/// <summary>
		/// Convert BSON document object into BSON binary data byte array.
		/// </summary>
		/// <returns>The byte array.</returns>
		public byte[] ToByteArray() {
			byte[] res;
			using (var ms = new MemoryStream()) {
				Serialize(ms);
				res = ms.ToArray();
			}
			return res;
		}

		public string ToDebugDataString() {
			return BitConverter.ToString(ToByteArray());
		}

		/// <summary>
		/// Gets the field value.
		/// </summary>
		/// <returns>The BSON value.</returns>
		/// <see cref="Ejdb.BSON.BSONValue"/>
		/// <param name="key">Document field name</param>
		public BSONValue GetBSONValue(string key) {
			CheckFields();
			BSONValue ov;
			if (_fields.TryGetValue(key, out ov)) {
				return ov;
			} else {
				return null;
			}
		}

		/// <summary>
		/// Gets the field value object. 
		/// </summary>
		/// <remarks>
		/// Hierarchical field paths are NOT supported. Use <c>[]</c> operator instead.
		/// </remarks>
		/// <param name="key">BSON document key</param>
		/// <see cref="Ejdb.BSON.BSONValue"/>
		public object GetObjectValue(string key) {
			var bv = GetBSONValue(key);
			return bv != null ? bv.Value : null;
		}

		/// <summary>
		/// Determines whether this document has the specified key.
		/// </summary>
		/// <returns><c>true</c> if this document has the specified key; otherwise, <c>false</c>.</returns>
		/// <param name="key">Key.</param>
		public bool HasKey(string key) {
			return (GetBSONValue(key) != null);
		}

		/// <summary>
		/// Gets the <see cref="Ejdb.BSON.BSONDocument"/> with the specified key.
		/// </summary>
		/// <remarks>
		/// Getter for hierarchical field paths are supported.
		/// </remarks>
		/// <param name="key">Key.</param>
		/// <returns>Key object </c> or <c>null</c> if the key is not exists or value type is either 
		/// <see cref="Ejdb.BSON.BSONType.NULL"/> or <see cref="Ejdb.BSON.BSONType.UNDEFINED"/></returns>
		public object this[string key] {
			get {
				int ind;
				if ((ind = key.IndexOf(".", StringComparison.Ordinal)) == -1) {
					return GetObjectValue(key);
				} else {
					string prefix = key.Substring(0, ind);
					BSONDocument doc = GetObjectValue(prefix) as BSONDocument;
					if (doc == null || key.Length < ind + 2) {
						return null;
					}
					return doc[key.Substring(ind + 1)];
				}
			}
			set {
				object v = value;
				if (v == null) {
					SetNull(key);
					return;
				}
				Action<BSONDocument, string, object> setter;
				Type vtype = v.GetType();
				TYPE_SETTERS.TryGetValue(vtype, out setter);
				if (setter == null) {
					if (vtype.IsAnonymousType()) {
						setter = SetAnonType;
					} else {
						throw new Exception(string.Format("Unsupported value type: {0} for doc[key] assign operation", v.GetType()));
					}
				}
				setter(this, key, v);
			}
		}

		public static void SetAnonType(BSONDocument doc, string key, object val) {
			BSONDocument ndoc = (key == null) ? doc : new BSONDocument();
			Type vtype = val.GetType();
			foreach (PropertyInfo pi in vtype.GetProperties()) {
				if (!pi.CanRead) {
					continue;
				}
				ndoc[pi.Name] = pi.GetValue(val, null);
			}
			if (key != null) {
				doc.SetDocument(key, ndoc);
			}
		}

		public BSONDocument SetNull(string key) {
			return SetBSONValue(new BSONValue(BSONType.NULL, key));
		}

		public BSONDocument SetUndefined(string key) {
			return SetBSONValue(new BSONValue(BSONType.UNKNOWN, key));
		}

		public BSONDocument SetMaxKey(string key) {
			return SetBSONValue(new BSONValue(BSONType.MAXKEY, key));
		}

		public BSONDocument SetMinKey(string key) {
			return SetBSONValue(new BSONValue(BSONType.MINKEY, key));
		}

		public BSONDocument SetOID(string key, string oid) {
			return SetBSONValue(new BSONValue(BSONType.OID, key, new BSONOid(oid)));
		}

		public BSONDocument SetOID(string key, BSONOid oid) {
			return SetBSONValue(new BSONValue(BSONType.OID, key, oid));
		}

		public BSONDocument SetBool(string key, bool val) {
			return SetBSONValue(new BSONValue(BSONType.BOOL, key, val));
		}

		public BSONDocument SetNumber(string key, int val) {
			return SetBSONValue(new BSONValue(BSONType.INT, key, val));
		}

		public BSONDocument SetNumber(string key, long val) {
			return SetBSONValue(new BSONValue(BSONType.LONG, key, val));
		}

		public BSONDocument SetNumber(string key, double val) {
			return SetBSONValue(new BSONValue(BSONType.DOUBLE, key, val));
		}

		public BSONDocument SetNumber(string key, float val) {
			return SetBSONValue(new BSONValue(BSONType.DOUBLE, key, val));
		}

		public BSONDocument SetString(string key, string val) {
			return SetBSONValue(new BSONValue(BSONType.STRING, key, val));
		}

		public BSONDocument SetCode(string key, string val) {
			return SetBSONValue(new BSONValue(BSONType.CODE, key, val));
		}

		public BSONDocument SetSymbol(string key, string val) {
			return SetBSONValue(new BSONValue(BSONType.SYMBOL, key, val));
		}

		public BSONDocument SetDate(string key, DateTime val) {
			return SetBSONValue(new BSONValue(BSONType.DATE, key, val));
		}

		public BSONDocument SetRegexp(string key, BSONRegexp val) {
			return SetBSONValue(new BSONValue(BSONType.REGEX, key, val));
		}

		public BSONDocument SetBinData(string key, BSONBinData val) {
			return SetBSONValue(new BSONValue(BSONType.BINDATA, key, val));
		}

		public BSONDocument SetDocument(string key, BSONDocument val) {
			return SetBSONValue(new BSONValue(BSONType.OBJECT, key, val));
		}

		public BSONDocument SetArray(string key, BSONArray val) {
			return SetBSONValue(new BSONValue(BSONType.ARRAY, key, val));
		}

		public BSONDocument SetTimestamp(string key, BSONTimestamp val) {
			return SetBSONValue(new BSONValue(BSONType.TIMESTAMP, key, val));
		}

		public BSONDocument SetCodeWScope(string key, BSONCodeWScope val) {
			return SetBSONValue(new BSONValue(BSONType.CODEWSCOPE, key, val));
		}

		public BSONValue DropValue(string key) {
			var bv = GetBSONValue(key);
			if (bv == null) {
				return bv;
			}
			_cachedhash = null;
			_fields.Remove(key);
			_fieldslist.RemoveAll(x => x.Key == key);
			return bv;
		}

		/// <summary>
		/// Removes all data from document.
		/// </summary>
		public void Clear() {
			_cachedhash = null;
			_fieldslist.Clear();
			if (_fields != null) {
				_fields.Clear();
				_fields = null;
			}
		}

		public void Serialize(Stream os) {
			if (os.CanSeek) {
				long start = os.Position;
				os.Position += 4; //skip int32 document size
				using (var bw = new ExtBinaryWriter(os, Encoding.UTF8, true)) {
					foreach (BSONValue bv in _fieldslist) {
						WriteBSONValue(bv, bw);
					}
					bw.Write((byte) 0x00);
					long end = os.Position;
					os.Position = start;
					bw.Write((int) (end - start));
					os.Position = end; //go to the end
				}
			} else {
				byte[] darr;
				var ms = new MemoryStream();
				using (var bw = new ExtBinaryWriter(ms)) {
					foreach (BSONValue bv in _fieldslist) {
						WriteBSONValue(bv, bw);
					}
					darr = ms.ToArray();
				}	
				using (var bw = new ExtBinaryWriter(os, Encoding.UTF8, true)) {
					bw.Write(darr.Length + 4/*doclen*/ + 1/*0x00*/);
					bw.Write(darr);
					bw.Write((byte) 0x00); 
				}
			}
			os.Flush();
		}

		public override bool Equals(object obj) {
			if (obj == null) {
				return false;
			}
			if (ReferenceEquals(this, obj)) {
				return true;
			}
			if (!(obj is BSONDocument)) {
				return false;
			}
			BSONDocument d1 = this;
			BSONDocument d2 = ((BSONDocument) obj);
			if (d1.KeysCount != d2.KeysCount) {
				return false;
			}
			foreach (BSONValue bv1 in d1._fieldslist) {
				BSONValue bv2 = d2.GetBSONValue(bv1.Key);
				if (bv1 != bv2) {
					return false;
				}
			}
			return true;
		}

		public override int GetHashCode() {
			if (_cachedhash != null) {
				return (int) _cachedhash;
			}
			unchecked {
				int hash = 1;
				foreach (var bv in _fieldslist) {
					hash = (hash * 31) + bv.GetHashCode(); 
				}
				_cachedhash = hash;
			}
			return (int) _cachedhash;
		}

		public static bool operator ==(BSONDocument d1, BSONDocument d2) {
			return Equals(d1, d2);
		}

		public static bool operator !=(BSONDocument d1, BSONDocument d2) {
			return !(d1 == d2);
		}

		public static BSONDocument ValueOf(object val) {
			if (val == null) {
				return new BSONDocument();
			}
			Type vtype = val.GetType();
			if (val is BSONDocument) {
				return (BSONDocument) val;
			} else if (vtype == typeof(byte[])) {
				return new BSONDocument((byte[]) val);
			} else if (vtype.IsAnonymousType()) {
				BSONDocument doc = new BSONDocument();
				SetAnonType(doc, null, val);
				return doc;
			} 
			throw new InvalidCastException(string.Format("Unsupported cast type: {0}", vtype));
		}

		public object Clone() {
			return new BSONDocument(this);
		}

		public override string ToString() {
			return string.Format("[{0}: {1}]", GetType().Name, 
			                     string.Join(", ", from bv in _fieldslist select bv.ToString())); 
		}
		//.//////////////////////////////////////////////////////////////////
		// 						Private staff										  
		//.//////////////////////////////////////////////////////////////////
		internal BSONDocument Add(BSONValue bv) {
			_cachedhash = null;
			_fieldslist.Add(bv);
			if (_fields != null) {
				_fields[bv.Key] = bv;
			}
			return this;
		}

		internal BSONDocument SetBSONValue(BSONValue val) {
			_cachedhash = null;
			CheckFields();
			if (val.BSONType == BSONType.STRING && val.Key == "_id") {
				val = new BSONValue(BSONType.OID, val.Key, new BSONOid((string) val.Value));
			}
			BSONValue ov;
			if (_fields.TryGetValue(val.Key, out ov)) {
				ov.Key = val.Key;
				ov.BSONType = val.BSONType;
				ov.Value = val.Value;
			} else {
				_fieldslist.Add(val);
				_fields.Add(val.Key, val);
			}
			return this;
		}

		protected virtual void CheckKey(string key) {
		}

		protected void WriteBSONValue(BSONValue bv, ExtBinaryWriter bw) {
			BSONType bt = bv.BSONType;
			switch (bt) {
				case BSONType.EOO:
					break;
				case BSONType.NULL:
				case BSONType.UNDEFINED:	
				case BSONType.MAXKEY:
				case BSONType.MINKEY:
					WriteTypeAndKey(bv, bw);
					break;
				case BSONType.OID:
					{
						WriteTypeAndKey(bv, bw);
						BSONOid oid = (BSONOid) bv.Value;
						Debug.Assert(oid._bytes.Length == 12);
						bw.Write(oid._bytes);
						break;
					}
				case BSONType.STRING:
				case BSONType.CODE:
				case BSONType.SYMBOL:										
					WriteTypeAndKey(bv, bw);
					bw.WriteBSONString((string) bv.Value);
					break;
				case BSONType.BOOL:
					WriteTypeAndKey(bv, bw);
					bw.Write((bool) bv.Value);
					break;
				case BSONType.INT:
					WriteTypeAndKey(bv, bw);
					bw.Write((int) bv.Value);
					break;
				case BSONType.LONG:
					WriteTypeAndKey(bv, bw);
					bw.Write((long) bv.Value);
					break;
				case BSONType.ARRAY:
				case BSONType.OBJECT:
					{					
						BSONDocument doc = (BSONDocument) bv.Value;
						WriteTypeAndKey(bv, bw);
						doc.Serialize(bw.BaseStream);
						break;
					}
				case BSONType.DATE:
					{	
						DateTime dt = (DateTime) bv.Value;
						var diff = dt.ToLocalTime() - BSONConstants.Epoch;
						long time = (long) Math.Floor(diff.TotalMilliseconds);
						WriteTypeAndKey(bv, bw);
						bw.Write(time);
						break;
					}
				case BSONType.DOUBLE:
					WriteTypeAndKey(bv, bw);
					bw.Write((double) bv.Value);
					break;	
				case BSONType.REGEX:
					{
						BSONRegexp rv = (BSONRegexp) bv.Value;		
						WriteTypeAndKey(bv, bw);
						bw.WriteCString(rv.Re ?? "");
						bw.WriteCString(rv.Opts ?? "");						
						break;
					}				
				case BSONType.BINDATA:
					{						
						BSONBinData bdata = (BSONBinData) bv.Value;
						WriteTypeAndKey(bv, bw);
						bw.Write(bdata.Data.Length);
						bw.Write(bdata.Subtype);
						bw.Write(bdata.Data);						
						break;		
					}
				case BSONType.DBREF:
					//Unsupported DBREF!
					break;
				case BSONType.TIMESTAMP:
					{
						BSONTimestamp ts = (BSONTimestamp) bv.Value;
						WriteTypeAndKey(bv, bw);
						bw.Write(ts.Inc);
						bw.Write(ts.Ts);
						break;										
					}		
				case BSONType.CODEWSCOPE:
					{
						BSONCodeWScope cw = (BSONCodeWScope) bv.Value;						
						WriteTypeAndKey(bv, bw);						
						using (var cwwr = new ExtBinaryWriter(new MemoryStream())) {							
							cwwr.WriteBSONString(cw.Code);
							cw.Scope.Serialize(cwwr.BaseStream);					
							byte[] cwdata = ((MemoryStream) cwwr.BaseStream).ToArray();
							bw.Write(cwdata.Length);
							bw.Write(cwdata);
						}
						break;
					}
				default:
					throw new InvalidBSONDataException("Unknown entry type: " + bt);											
			}		
		}

		protected void WriteTypeAndKey(BSONValue bv, ExtBinaryWriter bw) {
			bw.Write((byte) bv.BSONType);
			bw.WriteCString(bv.Key);
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
