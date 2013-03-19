local ejdb = require("ejdb")
local inspect = require("ejdb.inspect")
local Q = ejdb.Q

-- Used modes:
-- 'r' - read
-- 'w' - write
-- 'c' - create db if not exists
-- 't' - truncate existing db
local db = ejdb.open("zoo", "rwct")

-- Unordered lua table
local parrot1 = {
  name = "Grenny",
  type = "African Grey",
  male = true,
  age = 1,
  birthhdate = ejdb.toDateNow(),
  likes = { "green color", "night", "toys" },
  extra1 = ejdb.toNull()
}

-- Preserve order of BSON keys
local parrot2 = Q();
parrot2:KV("name", "Bounty"):KV("type", "Cockatoo"):KV("male", false)
parrot2:KV("age", 15):KV("birthdate",
  ejdb.toDate({ year = 2013, month = 1, day = 1, hour = 0, sec = 1 }))
parrot2:KV("likes", { "sugar cane" }):KV("extra1", ejdb.toNull())

--IF YOU WANT SOME DATA INSPECTIONS:
--print(ejdb.print_bson(parrot2:toBSON()))
--local obj = ejdb.from_bson(parrot2:toBSON())
--print(inspect(obj));

db:save("parrots2", parrot1)
db:save("parrots2", parrot2)

-- Below two equivalent queries:
-- Q1
local res, count, log =
db:find("parrots2", Q("likes", "toys"):OrderBy("name asc", "age desc"))
for i = 1, #res do -- iterate one
  local ob = res:object(i)
  print("" .. ob["name"] .. " likes toys #1")
end

-- Q2
local res, count, log =
db:find("parrots2", Q():F("likes"):Eq("toys"):OrderBy({ name = 1 }, { age = -1 }))
for i = 1, #res do -- iterate one
  print("" .. res:field(i, "name") .. " likes toys #2")
end

-- Second way to iterate
for vobj, idx in res() do
  print("" .. vobj["name"] .. " likes toys #3")
end

db:close()
