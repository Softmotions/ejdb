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
using Ejdb.DB;

namespace Ejdb.Tests {

	[TestFixture]
	public class TestEJDB {

		[Test]
		public void TestOpenClose() {
			EJDB jb = new EJDB("testdb1", EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC);
			Assert.IsTrue(jb.IsOpen);
			Assert.AreEqual(0, jb.LastDBErrorCode);
			Assert.AreEqual("success", jb.LastDBErrorMsg);
			jb.Dispose();
			Assert.IsFalse(jb.IsOpen);
			jb.Dispose(); //double dispose
		}




	}
}

