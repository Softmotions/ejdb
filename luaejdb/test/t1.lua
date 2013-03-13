

--package.path
package.path = "../?.lua;" .. package.path
package.cpath = "../?.so;" .. package.cpath

local inspect = require("inspect")
local ejdb = require("ejdb")
assert(type(ejdb) == "table")

local Q = ejdb.Q
local db = ejdb:open("testdb");
local q = Q("name", "Andy"):F("age"):Gt(20):Lt(40):F("score"):In({11, 22.12333, 1362835380447, db.toNull()}):Max(232);
--local q = Q("name", "Andy");

print(inspect(q._oarr))
print(db:find("mycoll", q))

db:close()



