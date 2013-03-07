require("bson");
local luaejdb = require("luaejdb")
local inspect = require("inspect")
assert(type(luaejdb) == "table")

local Q = {}
local mt_QO = {
  __index = Q
}

function Q:_init(fname, ...)
  self._field = nil --current field
  self._omap = {} --field operations
  self._or = {} -- OR joined restrictions
  self._ocnt = 1 -- field order counter
  self._skip = 0
  self._limit = 0
  if fname then
    self:F(fname, ...)
  end
  return self
end

function Q:_value(val)
  return val
end

function Q:_checkop()
  assert(type(self._field) == "string")
end

function Q:_setop(op, val, ...)
  self:_checkop()
  local types, replace = ...
  val = self:_toopval(val)
  local olist = self._omap[self._field]
  if not olist then
    olist = { self._ocnt };
    self._ocnt = self._ocnt
    self._omap[self._field] = olist
    self._ocnt = self._ocnt + 1
  elseif replace then
    olist = { olist[1] }
    self._omap[self._field] = olist
  end
  table.insert(olist, { op, val })
end

function Q:_toopval(val)
  return val
end

function Q:_rootOp(name, val)
  self:F(name)
  self:_setop(nil, val, nil, true)
  self:F(nil)
  return self
end

function Q:F(fname, ...)
  assert(fname == nil or type(fname) == "string")
  self._field = fname
  if #{ ... } == 1 then
    local v = ...
    return self:Eq(v)
  end
  return self
end

function Q:Eq(val) self:_setop(nil, val, nil, true) return self end

function Q:ElemMatch(val) self:_setop("$elemMatch", val) return self end

function Q:Not(val) self:_setop("$not", val) return self end

function Q:Gt(val) self:_setop("$gt", val) return self end

function Q:Gte(val) self:_setop("$gte", val) return self end

function Q:Lt(val) self:_setop("$lt", val) return self end

function Q:Lte(val) self:_setop("$lte", val) return self end

function Q:Icase(val) self:_setop("$icase", val) return self end

function Q:Begin(val) self:_setop("$begin", val) return self end

function Q:In(val) self:_setop("$in", val) return self end

function Q:NotIn(val) self:_setop("$nin", val) return self end

function Q:Bt(val) self:_setop("$bt", val) return self end

function Q:StrAnd(val) self:_setop("$strand", val) return self end

function Q:StrOr(val) self:_setop("$strand", val) return self end

function Q:Inc(val) self:_setop("$inc", val) return self end

function Q:Set(val) return self:_rootOp("$set", val) end

function Q:AddToSet(val) return self:_rootOp("$addToSet", val) end

function Q:AddToSetAll(val) return self:_rootOp("$addToSetAll", val) end

function Q:Pull(val) return self:_rootOp("$pull", val) end

function Q:PullAll(val) return self:_rootOp("$pullAll", val) end

function Q:Upsert(val) return self:_rootOp("$upsert", val) end

function Q:DropAll(val) return self:_rootOp("$dropall", val) end

function Q:Do(val) return self:_rootOp("$do", val) end

function Q:Or(...)
  for i, v in ipairs({ ... }) do
    local mt = getmetatable(v)
    assert(mt == mt_QO, "Each 'or' argument must be instance of 'luaejdb.Q' class")
    table.insert(self._or, v)
  end
  return self
end

function Q:toBSON()
  print(inspect(self._omap))
end

luaejdb["Q"] = setmetatable(Q, {
  __call = function(q, ...)
    local obj = {}
    setmetatable(obj, mt_QO)
    obj:_init(...)
    return obj;
  end;
})


local q = Q("name"):Eq("Anton"):F("age"):Eq(22)
q:toBSON()

local q = Q("name", "Anton"):F("age", 22):F("score"):Bt({1, 3})
q:toBSON()


local q = Q("name", "Anton"):F("age", 22):F("score"):Not():Bt({1, 3})
q:toBSON()



--local db = luaejdb.open("mydb", luaejdb.DEFAULT_OPEN_MODE);
--for k, v in pairs(db) do
--  print(k, v)
--end
--db:find("mycoll", { { name = "anton", age = { ["$gt"] = 2 } } })
--db:find("mycoll", "name=?, age>?, ")

--db.find(Q().F("name").Icase().In({1,2,3}).F("score").In({ 30, 231 }).Order("name", 1, "age", 2).Skip(10).Max(100));
--db:close()


--[[local o = {
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
assert ( bson.to_bson ( t ) == b )]]


return luaejdb;