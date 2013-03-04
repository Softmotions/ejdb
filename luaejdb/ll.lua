-- Library for reading low level data
-- Taken from https://github.com/daurnimator/mongol under MIT license

local assert = assert
local unpack = unpack
local floor = math.floor
local strbyte, strchar = string.byte, string.char

local ll = {}

local le_uint_to_num = function(s, i, j)
  i, j = i or 1, j or #s
  local b = { strbyte(s, i, j) }
  local n = 0
  for i = #b, 1, -1 do
    n = n * 2 ^ 8 + b[i]
  end
  return n
end
local le_int_to_num = function(s, i, j)
  i, j = i or 1, j or #s
  local n = le_uint_to_num(s, i, j)
  local overflow = 2 ^ (8 * (j - i) + 7)
  if n > 2 ^ overflow then
    n = -(n % 2 ^ overflow)
  end
  return n
end
local num_to_le_uint = function(n, bytes)
  bytes = bytes or 4
  local b = {}
  for i = 1, bytes do
    b[i], n = n % 2 ^ 8, floor(n / 2 ^ 8)
  end
  assert(n == 0)
  return strchar(unpack(b))
end
local num_to_le_int = function(n, bytes)
  bytes = bytes or 4
  if n < 0 then -- Converted to unsigned.
    n = 2 ^ (8 * bytes) + n
  end
  return num_to_le_uint(n, bytes)
end

local be_uint_to_num = function(s, i, j)
  i, j = i or 1, j or #s
  local b = { strbyte(s, i, j) }
  local n = 0
  for i = 1, #b do
    n = n * 2 ^ 8 + b[i]
  end
  return n
end
local num_to_be_uint = function(n, bytes)
  bytes = bytes or 4
  local b = {}
  for i = bytes, 1, -1 do
    b[i], n = n % 2 ^ 8, floor(n / 2 ^ 8)
  end
  assert(n == 0)
  return strchar(unpack(b))
end

-- Returns (as a number); bits i to j (indexed from 0)
local extract_bits = function(s, i, j)
  j = j or i
  local i_byte = floor(i / 8) + 1
  local j_byte = floor(j / 8) + 1

  local n = be_uint_to_num(s, i_byte, j_byte)
  n = n % 2 ^ (j_byte * 8 - i)
  n = floor(n / 2 ^ ((-(j + 1)) % 8))
  return n
end

-- Look at ith bit in given string (indexed from 0)
-- Returns boolean
local le_bpeek = function(s, bitnum)
  local byte = floor(bitnum / 8) + 1
  local bit = bitnum % 8
  local char = strbyte(s, byte)
  return floor((char % 2 ^ (bit + 1)) / 2 ^ bit) == 1
end
-- Test with:
--local sum = 0 for i=0,31 do v = le_bpeek ( num_to_le_uint ( N , 4 ) , i ) sum=sum + ( v and 1 or 0 )*2^i end assert ( sum == N )

local be_bpeek = function(s, bitnum)
  local byte = floor(bitnum / 8) + 1
  local bit = 7 - bitnum % 8
  local char = strbyte(s, byte)
  return floor((char % 2 ^ (bit + 1)) / 2 ^ bit) == 1
end
-- Test with:
--local sum = 0 for i=0,31 do v = be_bpeek ( num_to_be_uint ( N , 4 ) , i ) sum=sum + ( v and 1 or 0 )*2^(31-i) end assert ( sum == N )


local hasffi, ffi = pcall(require, "ffi")
local to_double, from_double
do
  local s, e, d
  if hasffi then
    d = ffi.new("double[1]")
  else
    -- Can't use with to_double as we can't strip debug info :(
    d = string.dump(loadstring([[return 523123.123145345]]))
    s, e = d:find("\3\54\208\25\126\204\237\31\65")
    s = d:sub(1, s)
    e = d:sub(e + 1, -1)
  end

  function to_double(n)
    if hasffi then
      d[0] = n
      return ffi.string(d, 8)
    else
      -- Should be the 8 bytes following the second \3 (LUA_TSTRING == '\3')
      local str = string.dump(loadstring([[return ]] .. n))
      local loc, en, mat = str:find("\3(........)", str:find("\3") + 1)
      return mat
    end
  end

  function from_double(str)
    assert(#str == 8)
    if hasffi then
      ffi.copy(d, str, 8)
      return d[0]
    else
      str = s .. str .. e
      return loadstring(str)()
    end
  end
end

return {
  le_uint_to_num = le_uint_to_num;
  le_int_to_num = le_int_to_num;
  num_to_le_uint = num_to_le_uint;
  num_to_le_int = num_to_le_int;

  be_uint_to_num = be_uint_to_num;
  num_to_be_uint = num_to_be_uint;

  extract_bits = extract_bits;

  le_bpeek = le_bpeek;
  be_bpeek = be_bpeek;

  to_double = to_double;
  from_double = from_double;
}