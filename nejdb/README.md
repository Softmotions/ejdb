[EJDB](http://ejdb.org) .Net Binding
===========================================


**Note: The .Net EJDB binding designed for .Net 4.0/4.5 and tested on Mono3/MSVC for Unix and Windows.**


Windows
--------------------------------

**Prerequisites**

 * EJDB C library >= v1.1.13
 * .Net >= 4.0 runtime
 * MSVS 2012 OR Xamarin studio (optional)

Download appropriate [EJDB binary distribution](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md).
Then add the directory containing the `tcejdbdll.dll` into search `PATH`.
Use the following solution configs to debug and test: `DebugWindows`, `ReleaseWindows`

If you have problems please follow this checklist:

**Windows checklist**

  0. Ensure .Net framework >= 4.0 installed
  1. For x86 CPU download [tcejdb-1.x.x-mingw32-i686](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)
  2. For x64 CPU download [tcejdb-1.x.x-mingw32-x86_64](https://github.com/Softmotions/ejdb/blob/master/tcejdb/WIN32.md)
  3. Ensure you have placed `tcejdbdll.dll` into the `%PATH%`
  4. Open the sample `nejdb.sln` solution.
  5. Ensure that the project configutation is either: `DebugWindows` OR `ReleaseWindows`
  6. If a target platform CPU differs from current host CPU you have to use appropriated `tcejdbdll.dll` for target and
    change the project's CPU platform configuration.


Unix
---------------------------------

**Prerequisites**
 * EJDB C library >= v1.1.13
 * Mono 3.0 runtime
 * Monodeveloper (optional)

Install the tcejdb >= 1.1.13 as system-wide library.
The `tcejdb.so` shared library should be visible to the system linker.
Use the following solution configs to debug and test: `DebugUnix`, `ReleaseUnix`


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
