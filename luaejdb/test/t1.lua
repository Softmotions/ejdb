

--package.path
package.path = "../?.lua;" .. package.path
package.cpath = "../?.so;" .. package.cpath

local inspect = require("inspect")
local ejdb = require("ejdb")
local bson = require("bson")
assert(type(ejdb) == "table")

local Q = ejdb.Q

local db = ejdb:open("testdb");
local q = Q("name", "J"):F("age"):Gt(20):F("score"):In({11, 22.12333, 1362835380447}):Max(232);

--print("Q", inspect(q._omap))
--local bsd = q:toBSON()
--local js = bson.from_bson(bson.string_source(bsd))
--print("J", inspect(js))
--print("I", bsd)

print(db:find("mycoll", q))
db:close()



