-- Taken from https://github.com/daurnimator/mongol under MIT license

local modname = (...)
local M = {}

local ll = require("ll")

_G[modname] = M
package.loaded[modname] = M
setmetatable(M, { __index = _G })
if type(_ENV) == "table" then
  _ENV = M;
else
  setfenv(1, M)
end

local binary_mt = {}
local utc_date = {}
local regexp_mt = {}
local object_id_mt = {
  __tostring = function(ob)
    local t = {}
    for i = 1, 12 do
      table.insert(t, string.format("%02x", string.byte(ob.id, i, i)))
    end
    return table.concat(t)
  end;
  __eq = function(a, b) return a.id == b.id end;
}

function string_source(s, i)
  i = i or 1
  return function(n)
    if not n then -- Rest of string
      n = #s - i + 1
    end
    i = i + n
    assert(i - 1 <= #s, "Unable to read enough characters")
    return string.sub(s, i - n, i - 1)
  end, function(new_i)
    if new_i then i = new_i end
    return i
  end
end


function object_id_from_string(str)
  assert(type(str) == "string" and #str == 24, string.format("Invalid object id string %s", str));
  local t = {}
  for i = 1, 24, 2 do
    local n = tonumber(string.sub(str, i, i + 1), 16)
    assert(n ~= nil, string.format("Invalid object id string %s", str));
    table.insert(t, string.char(n))
  end
  return table.concat(t)
end

function object_id(str)
  assert(#str == 12)
  return setmetatable({ id = str }, object_id_mt)
end

local function string_to_array_of_chars(s)
  local t = {}
  for i = 1, #s do
    t[i] = string.sub(s, i, i)
  end
  return t
end

local function read_terminated_string(get, terminators)
  local terminators = string_to_array_of_chars(terminators or "\0")
  local str = {}
  local found = 0
  while found < #terminators do
    local c = get(1)
    if c == terminators[found + 1] then
      found = found + 1
    else
      found = 0
    end
    table.insert(str, c)
  end
  return table.concat(str, "", 1, #str - #terminators)
end


local function read_document(get, numerical)
  local bytes = ll.le_uint_to_num(get(4))
  local ho, hk, hv = false, false, false
  local t = {}
  while true do
    local op = get(1)
    if op == "\0" then break end

    local e_name = read_terminated_string(get)
    local v
    if op == "\1" then -- Double
      v = ll.from_double(get(8))
    elseif op == "\2" then -- String
      local len = ll.le_uint_to_num(get(4))
      v = get(len - 1)
      assert(get(1) == "\0")
    elseif op == "\3" then -- Embedded document
      v = read_document(get, false)
    elseif op == "\4" then -- Array
      v = read_document(get, true)
    elseif op == "\5" then -- Binary
      local len = ll.le_uint_to_num(get(4))
      local subtype = get(1)
      v = get(len)
    elseif op == "\7" then -- ObjectId
      v = object_id(get(12))
    elseif op == "\8" then -- false
      local f = get(1)
      if f == "\0" then
        v = false
      elseif f == "\1" then
        v = true
      else
        error(f:byte())
      end
    elseif op == "\9" then -- UTC datetime milliseconds
      v = ll.le_uint_to_num(get(8), 1, 8)
    elseif op == "\10" then -- Null
      v = nil
    elseif op == "\11" then -- Regexp
      local re = read_terminated_string(get)
      local opts = read_terminated_string(get)
      v = setmetatable({re = re, opts = opts}, regexp_mt)
    elseif op == "\16" then --int32
      v = ll.le_int_to_num(get(4), 1, 8)
    elseif op == "\17" then --int64
      v = ll.le_int_to_num(get(8), 1, 8)
    elseif op == "\18" then --int64
      v = ll.le_int_to_num(get(8), 1, 8)
    else
      error("Unknown BSON type: " .. string.byte(op))
    end

    if numerical then
      t[tonumber(e_name)] = v
    else
      t[e_name] = v
    end

    -- Check for special universal map
    if e_name == "_keys" then
      hk = v
    elseif e_name == "_vals" then
      hv = v
    else
      ho = true
    end
  end

  if not ho and hk and hv then
    t = {}
    for i = 1, #hk do
      t[hk[i]] = hv[i]
    end
  end

  return t
end

function get_utc_date(v)
  return setmetatable({ v = v }, utc_date)
end

function get_bin_data(v)
  return setmetatable({ v = v, st = "\0" }, binary_mt)
end

function from_bson(get)
  local t = read_document(get, false)
  return t
end

local function pack(k, v)
  local ot = type(v)
  local mt = getmetatable(v)

  if ot == "number" then
    return "\1" .. k .. "\0" .. ll.to_double(v)
  elseif ot == "nil" then
    return "\10" .. k .. "\0"
  elseif ot == "string" then
    return "\2" .. k .. "\0" .. ll.num_to_le_uint(#v + 1) .. v .. "\0"
  elseif ot == "boolean" then
    if v == false then
      return "\8" .. k .. "\0\0"
    else
      return "\8" .. k .. "\0\1"
    end
  elseif mt == object_id_mt then
    return "\7" .. k .. "\0" .. v.id
  elseif mt == utc_date then
    return "\9" .. k .. "\0" .. ll.num_to_le_int(v.v, 8)
  elseif mt == binary_mt then
    return "\5" .. k .. "\0" .. ll.num_to_le_uint(string.len(v.v)) ..
           v.st .. v.v
  elseif ot == "table" then
    local doc, array = to_bson(v)
    if array then
      return "\4" .. k .. "\0" .. doc
    else
      return "\3" .. k .. "\0" .. doc
    end
  else
    error("Failure converting " .. ot .. ": " .. tostring(v))
  end
end

function to_bson(ob)
  -- Find out if ob if an array; string->value map; or general table
  local onlyarray = true
  local seen_n, high_n = {}, 0
  local onlystring = true
  for k, v in pairs(ob) do
    local t_k = type(k)
    onlystring = onlystring and (t_k == "string")
    if onlyarray then
      if t_k == "number" and k >= 0 then
        if k >= high_n then
          high_n = k
          seen_n[k] = v
        end
      else
        onlyarray = false
      end
    end
    if not onlyarray and not onlystring then break end
  end

  local retarray, m = false
  if onlystring then -- Do string first so the case of an empty table is done properly
    local r = {}
    for k, v in pairs(ob) do
      table.insert(r, pack(k, v))
    end
    m = table.concat(r)
  elseif onlyarray then
    local r = {}
    local low = 0
    --if seen_n [ 0 ] then low = 0 end
    for i = low, high_n do
      r[i] = pack(i, seen_n[i])
    end
    m = table.concat(r, "", low, high_n)
    retarray = true
  else
    local ni = 1
    local keys, vals = {}, {}
    for k, v in pairs(ob) do
      keys[ni] = k
      vals[ni] = v
      ni = ni + 1
    end
    return to_bson({ _keys = keys, _vals = vals })
  end

  return ll.num_to_le_uint(#m + 4 + 1) .. m .. "\0", retarray
end
