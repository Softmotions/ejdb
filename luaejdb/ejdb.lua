local luaejdb = require("luaejdb")
assert(type(luaejdb) == "table")
local inspect = require("ejdb.inspect")

-- ------------ Misc -----------------------

local function strim(s)
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

local mtBSON_INT = {
  __bsontype = luaejdb.BSON_INT
}

local mtBSON_DOUBLE = {
  __bsontype = luaejdb.BSON_DOUBLE
}

local mtBSON_LONG = {
  __bsontype = luaejdb.BSON_LONG
}

local mtBSON_OBJECT = {
  __bsontype = luaejdb.BSON_OBJECT
}

local mtBSON_BOOL = {
  __bsontype = luaejdb.BSON_BOOL
}


function luaejdb.toOID(val)
  luaejdb.check_valid_oid_string(val)
  return setmetatable({ val }, mtBSON_OID);
end

function luaejdb.toDate(val)
  if type(val) == "table" then
    val = os.time(val) * 1000
  end
  assert(type(val) == "number")
  return setmetatable({ val }, mtBSON_DATE);
end

function luaejdb.toDateNow()
  return luaejdb.toDate(os.time() * 1000)
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
function luaejdb.open(path, omode, ...)
  return setmetatable(luaejdb_open(path, omode), mtDBObj)
end


function DB:command(cmd)
  assert(cmd and type(cmd) == "table", "Invalid arg #1")
  if getmetatable(cmd) == mtBObj then
    cmd = cmd:toBSON()
  else
    cmd = luaejdb.to_bson(cmd)
  end
  return self:_command(cmd)
end


function DB:save(cname, obj, ...)
  assert(obj and type(obj) == "table", "Invalid arg #2")
  if getmetatable(obj) == mtBObj then
    obj = obj:toBSON()
  else
    obj = luaejdb.to_bson(obj)
  end
  return self:_save(cname, obj, ...)
end

function DB:find(cname, q, ...)
  assert(getmetatable(q) == mtBObj, "Query object must be instance of 'luaejdb.Q' class `q = luaejdb.Q()`")
  local sflags = ...
  local orBsons = {}
  local ors = q:getJoinedORs()
  if ors then
    for _, o in ipairs(ors) do
      table.insert(orBsons, o:toBSON())
    end
  end
  return self:_find(cname, q:toBSON(), orBsons, q:toHintsBSON(), sflags)
end


function DB:findOne(cname, q, ...)
  assert(getmetatable(q) == mtBObj, "Query object must be instance of 'luaejdb.Q' class `q = luaejdb.Q()`")
  q:Max(1);
  local qres, count, log = self:find(cname, q, ...)
  if qres ~= nil and #qres > 0 then
    return qres:object(1), count, log
  else
    return nil, count, log
  end
end

function DB:count(cname, q, sflags, ...)
  if type(sflags) ~= "string" then
    sflags = "c"
  else
    sflags = "c" .. sflags --append count flag
  end
  local _, count, log = self:find(cname, q, sflags, ...)
  return count, log
end

function DB:update(cname, q, ...)
  return self:count(cname, q, ...)
end

function DB:dropIndexes(cname, fpath)
  return self:_setIndex(cname, fpath, "", "a")
end

function DB:optimizeIndexes(cname, fpath)
  return self:_setIndex(cname, fpath, "", "o")
end

function DB:ensureStringIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "s", "")
end

function DB:rebuildStringIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "s", "r")
end

function DB:dropStringIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "s", "d")
end

function DB:ensureIStringIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "i", "")
end

function DB:rebuildIStringIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "i", "r")
end

function DB:dropIStringIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "i", "d")
end

function DB:ensureNumberIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "n", "")
end

function DB:rebuildNumberIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "n", "r")
end

function DB:dropNumberIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "n", "d")
end

function DB:ensureArrayIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "a", "")
end

function DB:rebuildArrayIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "a", "r")
end

function DB:dropArrayIndex(cname, fpath)
  return self:_setIndex(cname, fpath, "a", "d")
end

function DB:beginTransaction(cname)
  return self:_txctl(cname, "b")
end

function DB:commitTransaction(cname)
  return self:_txctl(cname, "c")
end

function DB:rollbackTransaction(cname)
  return self:_txctl(cname, "a")
end

function DB:getTransactionStatus(cname)
  return self:_txctl(cname, "s")
end

-- ------- EJDB Query  -------------

function B:_init(fname, ...)
  self._field = nil -- current field
  self._or = {} -- OR joined restrictions
  self._hints = nil -- hints Q
  self._omap = {} -- field operations
  self._oarr = {} -- resulting array of field operations
  self._bson = nil -- cached bson
  if fname then
    self:F(fname, ...)
  end
  return self
end

function B:_checkOp()
  assert(type(self._field) == "string")
end

function B:_setOp(op, val, ...)
  self:_checkOp()
  local types, replace = ...
  local ttypes = type(types)
  if (ttypes == "string") then
    assert(type(val) == types, "Invalid query argument field: " .. self._field ..
                               " It should have '" .. types .. "' type," .. " got: " .. inspect(val))
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
  if op == nil then
    table.insert(olist, setmetatable({ val }, mtBVal))
  else
    local found = false
    for i = 2, #olist do
      if olist[i][1] == op then -- found previous added op
        found = true
        olist[i][2] = val -- replace old value
        break
      end
    end
    if not found then
      table.insert(olist, { op, val })
    end
  end
  self._bson = nil
  return self
end

function B:_invertOp(op, val, ...)
  local pf = self._field;
  assert(type(op) == "string", "Operation must be a string")
  assert(type(pf) == "string", "You should set field before by Q:F('fname')")
  self._field = op
  self:_setOp(pf, val, ...)
  self._field = pf
  return self
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
  return self
end

function B:_rootOp(name, val, ...)
  local types = ...
  self:F(name)
  self:_setOp(nil, val, types, true)
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
function B:KV(key, val)
  self:F(key);
  self:_setOp(nil, val, nil, true)
  return self
end

function B:Eq(val) return self:_setOp(nil, val, nil, true) end

function B:ElemMatch(val) return self:_setOp("$elemMatch", val) end

function B:Not(val) return self:_setOp("$not", val) end

function B:Gt(val) return self:_setOp("$gt", val, "number") end

function B:Gte(val) return self:_setOp("$gte", val, "number") end

function B:Lt(val) return self:_setOp("$lt", val, "number") end

function B:Lte(val) return self:_setOp("$lte", val, "number") end

function B:Icase(val) return self:_setOp("$icase", val) end

function B:Begin(val) return self:_setOp("$begin", val, "string") end

function B:In(val) return self:_setOp("$in", val, "table") end

function B:NotIn(val) return self:_setOp("$nin", val, "table") end

function B:Bt(n1, n2) return self:_setOp("$bt", { n1, n2 }) end

function B:StrAnd(val) return self:_setOp("$strand", val, "table") end

function B:StrOr(val) return self:_setOp("$stror", val, "table") end

function B:Inc(val) return self:_invertOp("$inc", val, "number") end

function B:Set(val) return self:_rootOp("$set", val, "table") end

function B:AddToSet(val) return self:_invertOp("$addToSet", val) end

function B:AddToSetAll(val) return self:_invertOp("$addToSetAll", val, "table") end

function B:Pull(val) return self:_invertOp("$pull", val) end

function B:PullAll(val) return self:_invertOp("$pullAll", val, "table")  end

function B:Upsert(val) return self:_rootOp("$upsert", val, "table") end

function B:DropAll() return self:_rootOp("$dropall", true) end

function B:Do(val) return self:_rootOp("$do", val, "table") end

function B:Join(cname, fpath)
  assert(type(cname) == "string", "Type of #1 arg must be string")
  assert(type(fpath) == "string", "Type of #2 arg must be string")
  return self:_rootOp("$do", { [fpath] = { ["join"] = cname } })
end

function B:Or(...)
  self._or = self._or or {}
  for i, v in ipairs({ ... }) do
    assert(getmetatable(v) == mtBObj, "Each 'Or()' argument must be instance of 'luaejdb.B' class")
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
  if self._bson then
    return self._bson
  else
    self._bson = luaejdb.to_bson(self)
    return self._bson
  end
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