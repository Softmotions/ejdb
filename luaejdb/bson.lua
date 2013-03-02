--**************************************************************************************************
--  Python API for EJDB database library http://ejdb.org
--  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
--
--  This file is part of EJDB.
--  EJDB is free software; you can redistribute it and/or modify it under the terms of
--  the GNU Lesser General Public License as published by the Free Software Foundation; either
--  version 2.1 of the License or any later version.  EJDB is distributed in the hope
--  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
--  License for more details.
--  You should have received a copy of the GNU Lesser General Public License along with EJDB;
--  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
--  Boston, MA 02111-1307 USA.
-- *************************************************************************************************/


--[["BSON_Value",
   "BSON_Double",
   "BSON_String",
   "BSON_Document",
   "BSON_Array",
   "BSON_Binary",
     "BSON_Binary_Generic",
     "BSON_Binary_Function",
     "BSON_Binary_UUID",
     "BSON_Binary_MD5",
     "BSON_Binary_UserDefined",
   "BSON_ObjectId",
   "BSON_Boolean",
   "BSON_Datetime",
   "BSON_Null",
   "BSON_Regex",
   "BSON_JavaScript",
   "BSON_Symbol",
   "BSON_JavaScriptWithScope",
   "BSON_Int32",
   "BSON_Timestamp",
   "BSON_Int64",]]

--BSONValue = {}
--BSONString = {}

local modname = "bson"
local M = {}
_G[modname] = M
package.loaded[modname] = M
setmetatable(M, { __index = _G })
_ENV = M;

local typeHandlers = {}

BSONValue = {}

BSONString = {
  new = function(self, o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
  end
}





print("TYPE=" .. type(1223))

