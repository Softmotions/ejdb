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
using NUnit.Framework;
using Ejdb.SON;
using System.IO;

namespace Ejdb.Tests {

	[TestFixture]
	public class TestBSON {

		[Test]
		public void TestSerializeEmpty() {
			BSONDocument doc = new BSONDocument();
			Assert.AreEqual("05-00-00-00-00", doc.ToDebugDataString()); 
		}

		[Test]
		public void TestSerialize1() {
			byte[] bdata;
			BSONDocument doc = new BSONDocument();
			doc.SetNumber("0", 1);
			//0C-00-00-00 	len
			//10		  	type
			//30-00 		key
			//01-00-00-00	int val
			//00			zero term
			bdata = doc.ToByteArray();
			Assert.AreEqual("0C-00-00-00-10-30-00-01-00-00-00-00", doc.ToDebugDataString());
			Assert.AreEqual(bdata.Length, (int) Convert.ToByte(doc.ToDebugDataString().Substring(0, 2), 16));

			BSONDocument doc2 = new BSONDocument(doc.ToByteArray());
			Assert.AreEqual(1, doc2.KeysCount);
			int c = 0;
			foreach (BSONValue bv in doc2) {
				c++;		
				Assert.IsNotNull(bv);
				Assert.AreEqual(BSONType.INT, bv.BSONType);
				Assert.AreEqual("0", bv.Key);
				Assert.AreEqual(1, bv.Value);
			}
			Assert.That(c > 0);
			doc2.SetNumber("0", 2);
			Assert.AreEqual(1, doc2.KeysCount);
			object ival = doc2["0"];
			Assert.IsInstanceOfType(typeof(int), ival);
			Assert.AreEqual(2, ival);

			doc2.SetNumber("1", Int32.MaxValue);
			//13-00-00-00
			//10
			//30-00
			//02-00-00-00
			//10-31-00
			//FF-FF-FF-7F
			//00
			Assert.AreEqual("13-00-00-00-10-30-00-02-00-00-00-10-31-00-FF-FF-FF-7F-00", 
			                doc2.ToDebugDataString());

			doc2 = new BSONDocument(doc2);
			Assert.AreEqual("13-00-00-00-10-30-00-02-00-00-00-10-31-00-FF-FF-FF-7F-00", 
			                doc2.ToDebugDataString());

			doc2 = new BSONDocument(doc2.ToByteArray());
			Assert.AreEqual("13-00-00-00-10-30-00-02-00-00-00-10-31-00-FF-FF-FF-7F-00", 
			                doc2.ToDebugDataString());


			doc = new BSONDocument();
			doc["a"] = 1;
			Assert.AreEqual("0C-00-00-00-10-61-00-01-00-00-00-00", doc.ToDebugDataString());
		}
	}
}

