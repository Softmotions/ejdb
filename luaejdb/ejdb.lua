local luaejdb = require("luaejdb")
assert(type(luaejdb) == "table")
local inspect = require("inspect")

-- ------------ Misc -----------------------

function strim(s)
  local from = s:match("^%s*()")
  return from > #s and "" or s:match(".*%S", from)
end

-- ----------- Meta-tables ----------------

local B = {}
local mtBObj = {
  --Meta-table for JSON builder
  __index = B;
  __query = true
}
local mtBVal = {
  --Meta-table for internal JSON builder value
  __bval = true
}

-- BSON types markers
local mtBSON_OID = {
  __bsontype = luaejdb.BSON_OID
}

local mtBSON_DATE = {
  __bsontype = luaejdb.BSON_DATE
}

local mtBSON_REGEX = {
  __bsontype = luaejdb.BSON_REGEX
}

local mtBSON_BINDATA = {
  __bsontype = luaejdb.BSON_BINDATA
}

local mtBSON_NULL = {
  __bsontype = luaejdb.BSON_NULL
}

local mtBSON_UNDEFINED = {
  __bsontype = luaejdb.BSON_UNDEFINED
}

function luaejdb.toOID(val)
  assert(type(val) == "string" and #val == 24)
  return setmetatable({ val }, mtBSON_OID);
end

function luaejdb.toDate(val)
  assert(type(val) == "number")
  return setmetatable({ val }, mtBSON_DATE);
end

function luaejdb.toRegexp(re, opts)
  opts = opts or ""
  assert(type(re) == "string" and type(opts) == "string")
  return setmetatable({ re, opts }, mtBSON_REGEX);
end

function luaejdb.toBinData(val)
  assert(type(val) == "string")
  return setmetatable({ val }, mtBSON_BINDATA);
end

function luaejdb.toNull()
  return setmetatable({}, mtBSON_NULL);
end

function luaejdb.toUndefined()
  return setmetatable({}, mtBSON_UNDEFINED);
end


local DB = {
  toOID = luaejdb.toOID,
  toDate = luaejdb.toDate,
  toRegexp = luaejdb.toRegexp,
  toBinData = luaejdb.toBinData,
  toNull = luaejdb.toNull,
  toUndefined = luaejdb.toUndefined
}
local mtDBObj = {
  __index = DB
}

-- ------- EJDB DB ---------------------------

local luaejdb_open = luaejdb.open
function luaejdb:open(path, omode, ...)
  if type(omode) ~= "number" then
    omode = luaejdb.DEFAULT_OPEN_MODE
  end
  return setmetatable(luaejdb_open(path, omode), mtDBObj)
end

function DB:find(cname, q, ...)
  assert(getmetatable(q) == mtBObj, "Query object must be instance of 'luaejdb.B' class `q = luaejdb.B()`")
  local flags = ...
  if (type(flags) ~= "number") then
    flags = 0
  end
  local orBsons = {}
  for _, o in ipairs(q:getJoinedORs()) do
    table.insert(orBsons, o.toBSON())
  end
  return self:_find(cname, q:toBSON(), orBsons, q:toHintsBSON(), flags)
end

-- ------- EJDB Query  -------------

function B:_init(fname, ...)
  self._field = nil -- current field
  self._or = {} -- OR joined restrictions
  self._hints = nil -- hints Q
  self._omap = {} -- field operations
  self._oarr = {} -- resulting array of field operations
  if fname then
    self:F(fname, ...)
  end
  return self
end

function B:_value(val)
  return val
end

function B:_checkop()
  assert(type(self._field) == "string")
end

function B:_setop(op, val, ...)
  self:_checkop()
  local types, replace = ...
  local ttypes = type(types)
  if (ttypes == "string") then
    assert(type(val) == types, "Invalid query argument field: " .. self._field .. " val: " .. inspect(val))
  elseif (ttypes == "function") then
    assert(types(val), "Invalid query argument field: " .. self._field .. " val: " .. inspect(val))
  elseif (ttypes == "table") then
    local found = false
    local vtype = type(val)
    for _, t in ipairs(ttypes) do
      if (t == vtype) then
        found = true
        break
      end
    end
    if not found then
    end
  end
  if op == nil then
    replace = true
  end
  val = self:_toOpVal(op, val)
  local olist = self._omap[self._field]
  if not olist then
    olist = { self._field }
    self._omap[self._field] = olist
    self._oarr[#self._oarr + 1] = olist
  elseif replace then
    for i = 2, #olist do olist[i] = nil end
  end
  if (op == nil) then
    table.insert(olist, setmetatable({ val }, mtBVal))
  else
    table.insert(olist, { op, val })
  end
end

function B:_toOpVal(op, val)
  if op == nil and self._field == "_id" and type(val) == "string" then
    return luaejdb.toOID(val)
  else
    return val
  end
end

function B:_hintOp(op, val, ...)
  if not self._hints then
    self._hints = B()
  end
  self._hints:_rootOp(op, val, ...)
end

function B:_rootOp(name, val, ...)
  local types = ...
  self:F(name)
  self:_setop(nil, val, types, true)
  self:F(nil)
  return self
end

function B:F(fname, ...)
  assert(fname == nil or type(fname) == "string")
  self._field = fname
  if #{ ... } == 1 then
    local v = ...
    return self:Eq(v)
  end
  return self
end

-- Generic key=value
function B:KeyVal(key, val)
  self:F(key);
  self:_setop(nil, val, nil, true)
  return self
end

function B:Eq(val) self:_setop(nil, val, nil, true) return self end

function B:ElemMatch(val) self:_setop("$elemMatch", val) return self end

function B:Not(val) self:_setop("$not", val) return self end

function B:Gt(val) self:_setop("$gt", val) return self end

function B:Gte(val) self:_setop("$gte", val) return self end

function B:Lt(val) self:_setop("$lt", val) return self end

function B:Lte(val) self:_setop("$lte", val) return self end

function B:Icase(val) self:_setop("$icase", val) return self end

function B:Begin(val) self:_setop("$begin", val) return self end

function B:In(val) self:_setop("$in", val) return self end

function B:NotIn(val) self:_setop("$nin", val) return self end

function B:Bt(val) self:_setop("$bt", val) return self end

function B:StrAnd(val) self:_setop("$strand", val) return self end

function B:StrOr(val) self:_setop("$strand", val) return self end

function B:Inc(val) self:_setop("$inc", val) return self end

function B:Set(val) return self:_rootOp("$set", val) end

function B:AddToSet(val) return self:_rootOp("$addToSet", val) end

function B:AddToSetAll(val) return self:_rootOp("$addToSetAll", val) end

function B:Pull(val) return self:_rootOp("$pull", val) end

function B:PullAll(val) return self:_rootOp("$pullAll", val) end

function B:Upsert(val) return self:_rootOp("$upsert", val) end

function B:DropAll() return self:_rootOp("$dropall", true) end

function B:Do(val) return self:_rootOp("$do", val) end

function B:Or(...)
  for i, v in ipairs({ ... }) do
    assert(getmetatable(v) == mtBObj, "Each 'or' argument must be instance of 'luaejdb.B' class")
    table.insert(self._or, v)
  end
  return self
end

function B:Skip(val) self:_hintOp("$skip", val, "number") return self end

function B:Max(val) self:_hintOp("$max", val, "number") return self end

function B:OrderBy(...)
  local ospec = B()
  for _, v in ipairs({ ... }) do
    local key
    local oop = 1
    if type(v) == "string" then
      v = strim(v)
      if v:find(" desc", -5) or v:find(" DESC", -5) then
        oop = -1
        key = v:sub(1, -6)
      elseif v:find(" asc", -4) or v:find(" ASC", -4) then
        oop = 1
        key = v:sub(1, -5)
      else --ASC by default
        oop = 1
        key = v
      end
    elseif type(v) == "table" then
      for ok, ov in pairs(v) do
        key = ok
        oop = tonumber(ov) or 1
        break
      end
    end
    assert(type(key) == "string" and type(oop) == "number")
    ospec:F(key):Eq(oop)
  end
  self:_hintOp("$orderby", ospec)
  return self
end

function B:Fields(...)
  return self:_fields(1, ...)
end

function B:NotFields(...)
  return self:_fields(-1, ...)
end

function B:_fields(definc, ...)
  local fspec = B()
  for _, v in ipairs({ ... }) do
    local key
    local inc = definc
    if type(v) == "string" then
      key = v
      inc = definc
    elseif type(v) == "table" then
      for ok, ov in pairs(v) do
        key = ok
        inc = tonumber(ov) or definc
        break
      end
    end
    assert(type(key) == "string" and type(inc) == "number")
    fspec:F(key):Eq(inc)
  end
  self:_hintOp("$fields", fspec)
  return self
end

function B:getJoinedORs()
  return self._or
end

function B:toHintsBSON()
  return (self._hints or B()):toBSON()
end

function B:toBSON()
  return luaejdb.to_bson(self)
end

luaejdb.B = setmetatable(B, {
  __call = function(q, ...)
    local obj = {}
    setmetatable(obj, mtBObj)
    obj:_init(...)
    return obj;
  end;
})
luaejdb.Q = luaejdb.B -- Name variations

return luaejdb;