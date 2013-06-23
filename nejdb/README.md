
EJDB .Net Binding
===========================================


**Note: The .Net EJDB binding designed for .Net 4.0/4.5 and tested on Mono 3 for Unix and Windows.**


Prerequisites
--------------------------------

 * EJDB C library >= v1.1.13
 * Mono 3.0
 * Monodevelop 4.x


Unix
---------------------------------

Install the tcejdb >= 1.1.13 as system-wide library.
The `tcejdb.so` shared library should be visible to the system linker.
Use the following solution configs to debug and test: `DebugUnix`, `ReleaseUnix`


Windows
--------------------------------
Download appropriate [EJDB binary distribution](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md).
Then add the `tcejdbdll.dll` directory into search `PATH`.
Use the following solution configs to debug and test: `DebugWindows`, `ReleaseWindows`


One snippet intro
---------------------------------

```c#
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
					//fetch the `name` and the first element of likes array from the current BSON iterator.
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
```
