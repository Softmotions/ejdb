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
using Ejdb.DB;
using Ejdb.BSON;

namespace sample {

	class MainClass {

		public static void Main(string[] args) {
			var jb = new EJDB("zoo", EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC);
			jb.ThrowExceptionOnFail = true;

			var parrot1 = BSONDocument.ValueOf(new {
				name = "Grenny",
				type = "African Grey",
				male = true,
				age = 1,
				birthdate = DateTime.Now,
				likes = new string[] { "green color", "night", "toys" },
				extra = BSONull.VALUE
			});

			var parrot2 = BSONDocument.ValueOf(new {
				name = "Bounty",
				type = "Cockatoo",
				male = false,
				age = 15,
				birthdate = DateTime.Now,
				likes = new string[] { "sugar cane" }
			});

			jb.Save("parrots", parrot1, parrot2);

			Console.WriteLine("Grenny OID: " + parrot1["_id"]);
			Console.WriteLine("Bounty OID: " + parrot2["_id"]);

			var q = jb.CreateQuery(new {
				likes = "toys"
			}, "parrots").OrderBy("name");

			using (var cur = q.Find()) {
				Console.WriteLine("Found " + cur.Length + " parrots");
				foreach (var e in cur) {
					//fetch  the `name` and the first element of likes array from the current BSON iterator.
					//alternatively you can fetch whole document from the iterator: `e.ToBSONDocument()`
					BSONDocument rdoc = e.ToBSONDocument("name", "likes.0");	
					Console.WriteLine(string.Format("{0} likes the '{1}'", rdoc["name"], rdoc["likes.0"]));
				}
			}
			q.Dispose();
			jb.Dispose();
		}
	}
}
