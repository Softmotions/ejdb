require("bson");
local luaejdb = require("luaejdb")
assert(type(luaejdb) == "table")


local db = luaejdb.open("mydb", luaejdb.DEFAULT_OPEN_MODE);

for k, v  in pairs(db) do
   print(k, v)
end


db:close()

--[[
local o = {
  a = "lol";
  b = "foo";
  c = 42;
  d = { 5, 4, 3, 2, 1 };
  e = { { { {} } } };
  f = { [true] = { baz = "mars" } };
  g = bson.object_id("abcdefghijkl");
  r = bson.regexp("$.?", "g")
  --z = { [{}] = {} } ; -- Can't test as tables are unique
}

for k, v in pairs(o) do
   print("k=", k);
end

print("\n\n")

assert(tostring(bson.object_id(bson.object_id_from_string(tostring(o.g)))) == tostring(o.g))

local b = bson.to_bson(o)
local t = bson.from_bson(bson.string_source(b))

for k, v in pairs(t) do
   print("k=", k, "v=", v);
end

local function confirm ( orig , new , d )
	d = d or 1
	local ok = true
	for k ,v in pairs ( orig ) do
		local nv = new [ k ]
		--print(string.rep ( "\t" , d-1) , "KEY", type(k),k, "VAL",type(v),v,"NEWVAL",type(nv),nv)
		if nv == v then
		elseif type ( v ) == "table" and type ( nv ) == "table" then
			--print(string.rep ( "\t" , d-1) , "Descending" , k )
			ok = ok and confirm ( v , nv , d+1 )
		else
			print(string.rep ( "\t" , d-1) , "Failed on" , k , v , nv )
			ok = false
		end
	end
	return ok
end

assert ( confirm ( o , t ) )
assert ( bson.to_bson ( t ) == bson.to_bson ( t ) )
--assert ( bson.to_bson ( t ) == b )
]]





