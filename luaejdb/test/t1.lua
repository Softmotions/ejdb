--package.path
package.path = "../?.lua;" .. package.path
package.cpath = "../?.so;" .. package.cpath

local inspect = require("ejdb.inspect")
local ejdb = require("ejdb")
assert(type(ejdb) == "table")

assert(not pcall(function() ejdb.check_valid_oid_string("sss") end));
assert(pcall(function() ejdb.check_valid_oid_string("510f7fa91ad6270a00000000") end));

local Q = ejdb.Q
local B = ejdb.B

local db = ejdb.open("testdb", "rwct");
assert(db:isOpen() == true)
local q = Q("name", "Andy"):F("_id"):Eq("510f7fa91ad6270a00000000"):F("age"):Gt(20):Lt(40):F("score"):In({ 11, 22.12333, 1362835380447, db.toNull() }):Max(232);

assert([[.name(2)=Andy
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


]] == ejdb.print_bson(q:toBSON()))

local obj = ejdb.from_bson(q:toBSON())
--print(inspect(obj))
assert(obj.name == "Andy")
assert(obj._id == "510f7fa91ad6270a00000000")
assert(type(obj.age) == "table" and getmetatable(obj.age).__bsontype == db.BSON_OBJECT)
assert(obj["age"]["$gt"] == 20)
assert(obj["age"]["$lt"] == 40)
assert(type(obj.score) == "table" and getmetatable(obj.score).__bsontype == db.BSON_OBJECT)
assert(type(obj.score["$in"]) == "table" and getmetatable(obj.score["$in"]).__bsontype == db.BSON_ARRAY)
assert(#obj.score["$in"] == 4)

-- Second
--
q = Q("name", "Andy"):F("bdate"):Eq(db.toDate(1362835380447)):KV("car", Q("name", "Lamborghini"):KV("maxspeed", 320)):KV("dst", db.toNull());

assert([[.name(2)=Andy
.bdate(9)=1362835380447
.car(3)=
..name(2)=Lamborghini
..maxspeed(16)=320

.dst(10)=BSON_NULL
]] == ejdb.print_bson(q:toBSON()));

obj = ejdb.from_bson(q:toBSON())
assert(obj.name == "Andy")
assert(type(obj.bdate) == "table" and getmetatable(obj.bdate).__bsontype == db.BSON_DATE)
assert(obj.bdate[1] == 1362835380447)
assert(type(obj.dst) == "table" and getmetatable(obj.dst).__bsontype == db.BSON_NULL)

assert([[._id(7)=510f7fa91ad6270a00000000
.a(16)=2
.c(2)=d
.dd(3)=
..c(16)=1
..f(2)=v1
..gt(8)=true

.ee(2)=t
]] == ejdb.print_bson(ejdb.to_bson({ c = "d", a = 2, _id = "510f7fa91ad6270a00000000", dd = { f = "v1", gt = true, c = 1 }, ee = "t" })))

-- Test save
--
local oid = db:save("mycoll", { foo = "bar", k1 = "v1" });
local firstOid = oid;
ejdb.check_valid_oid_string(oid)

oid = db:save("mycoll", B("foo2", "bar2"):KV("g", "d"):KV("e", 1):KV("a", "g"));
ejdb.check_valid_oid_string(oid)

obj = db:load("mycoll", oid)
assert(type(obj) == "table")
assert(obj["_id"] == oid)
assert(obj.a == "g" and obj.e == 1 and obj.foo2 == "bar2" and obj.g == "d")

db:save("mycoll", { foo = "bar3" });
db:save("mycoll", { foo = "bar4" });
db:save("mycoll", { foo = "bar6", k2 = "v2" });

local qres, count, log = db:find("mycoll", Q("foo", "bar"))
assert(qres)
assert(count == 1)
assert(log == nil)
assert(#qres == 1)

for i = 1, #qres do
  local vobj = ejdb.from_bson(qres[i]);
  assert(vobj)
  assert(vobj["foo"] == "bar")
  assert(vobj["k1"] == "v1")
  ejdb.check_valid_oid_string(vobj["_id"])
end

for i = 1, #qres do
  assert(qres:field(i, "foo") == "bar")
  assert(qres:field(i, "k1") == "v1")
  local vobj = qres:object(i);
  assert(vobj)
  assert(vobj["foo"] == "bar")
  assert(vobj["k1"] == "v1")
  ejdb.check_valid_oid_string(vobj["_id"])
end

for vobj, idx in qres() do
  assert(idx == 1)
  assert(vobj["foo"] == "bar")
  assert(vobj["k1"] == "v1")
  ejdb.check_valid_oid_string(vobj["_id"])
end

for vbson, idx in qres("raw") do --iterate over bsons
  assert(idx == 1)
  assert(type(vbson) == "string");
  local vobj = ejdb.from_bson(vbson);
  assert(vobj)
  assert(vobj["foo"] == "bar")
  assert(vobj["k1"] == "v1")
  ejdb.check_valid_oid_string(vobj["_id"])
end

--test explist qres closing
assert(#qres == 1)
qres:close()
assert(#qres == 0)

local r, err = pcall(qres.object, qres, 1);
assert(err == "Cursor closed")

local qres, count, log = db:find("mycoll", Q("foo", "bar"), "l")
assert(type(log) == "string")
assert(qres ~= nil)
assert(#qres == 1)
assert(count == 1)
assert(log:find("COUNT ONLY: NO"))

local qres, count, log = db:find("mycoll", Q("foo", "bar"), "lc") --COUNT only
assert(type(log) == "string")
assert(qres == nil)
assert(count == 1)
assert(log:find("COUNT ONLY: YES"))

--db:save("mycoll", { foo = "bar6", k2 = "v2" });
local count, log = db:count("mycoll", Q():Or(Q("foo", "bar"), Q("foo", "bar6")));
assert(count == 2)
assert(log == nil)

local count, log = db:count("mycoll", Q():Or(Q("foo", "bar"), Q("foo", "bar6")), "l");
assert(count == 2)
assert(log:find("COUNT ONLY: YES"))

local vobj, count, log = db:findOne("mycoll", Q():Or(Q("foo", "bar"), Q("foo", "bar6")):OrderBy("foo desc"));
assert(count == 1)
assert(vobj["foo"] == "bar6")

local vobj, count, log = db:findOne("mycoll", Q():Or(Q("foo", "bar"), Q("foo", "bar6")):OrderBy({ foo = 1 }));
assert(count == 1)
assert(vobj["foo"] == "bar")

db:ensureCollection("ecoll", { large = true, records = 200000 })

assert(db:count("mycoll", Q("_id", firstOid)) == 1)
db:remove("mycoll", firstOid)
assert(db:count("mycoll", Q("_id", firstOid)) == 0)

db:sync("mycoll") -- sync only mycoll
db:sync() -- sync whole db

db:ensureStringIndex("mycoll", "foo")
local _, log = db:count("mycoll", Q("foo", "bar"), "l")
assert(log:find("MAIN IDX: 'sfoo'"))

db:dropIndexes("mycoll", "foo")

local _, log = db:count("mycoll", Q("foo", "bar"), "l")
assert(log:find("MAIN IDX: 'NONE'"))

assert(db:getTransactionStatus("mycoll") == false)
db:beginTransaction("mycoll")
assert(db:getTransactionStatus("mycoll") == true)
db:save("mycoll", { name = 1 })
assert(db:findOne("mycoll", Q("name", 1)));
db:rollbackTransaction("mycoll")
assert(db:getTransactionStatus("mycoll") == false)
assert(db:findOne("mycoll", Q("name", 1)) == nil);

assert(db:update("ecoll", Q("k1", "v1"):Upsert({ k1 = "v1", k2 = 1, k3 = 2 })) == 1)
assert(db:update("ecoll", Q("k1", "v1"):Upsert(Q("k1", "v2"))) == 1)
assert(db:count("ecoll", Q("k1", "v2")) == 1)

-- test $inc
assert(db:update("ecoll", Q("k1", "v2"):F("k2"):Inc(1):F("k3"):Inc(-2)) == 1);
assert(db:count("ecoll", Q():F("k2", 2):F("k3", 0)) == 1)

db:ensureStringIndex("mycoll", "foo")

--print(ejdb.print_bson(Q():F("tags"):AddToSetAll({"red", "green"}):toBSON()))
--print(inspect(db:getDBMeta()))

db:dropCollection("ecoll", true);

assert(db:count("ecoll", Q("k1", "v2")) == 0)

local cret = db:command({ping = {}});
assert(cret["log"] == "pong");


--print(ejdb.version());

assert(db:isOpen() == true)
db:close()
assert(db:isOpen() == false)

collectgarbage()
collectgarbage()
collectgarbage()
