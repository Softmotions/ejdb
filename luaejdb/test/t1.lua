

--package.path
package.path = "../?.lua;" .. package.path
package.cpath = "../?.so;" .. package.cpath

local inspect = require("inspect")
local ejdb = require("ejdb")
assert(type(ejdb) == "table")

local Q = ejdb.Q
local db = ejdb:open("testdb");
local q = Q("name", "Andy"):F("_id"):Eq("510f7fa91ad6270a00000000"):F("age"):Gt(20):Lt(40):F("score"):In({11, 22.12333, 1362835380447, db.toNull()}):Max(232);

local sv = ejdb.print_bson(q:toBSON());
local ev = [[.name(2)=Andy
._id(7)=510f7fa91ad6270a00000000
.age(3)=
..$gt(16)=20
..$lt(16)=40

.score(3)=
..$in(4)=
...1(16)=11
...2(1)=22.123330
...3(18)=1362835380447
...4(10)=BSON_NULL


]]
assert(sv == ev)

sv = q:toBSON();
print(inspect(ejdb.from_bson(sv)))



--print(inspect(q._oarr))
--print(db:find("mycoll", q))

db:close()



