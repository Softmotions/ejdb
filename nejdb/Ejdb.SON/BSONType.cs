// ============================================================================================
//   .NET API for EJDB database library http://ejdb.org
//   Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
//
//   This file is part of EJDB.
//   EJDB is free software; you can redistribute it and/or modify it under the terms of
//   the GNU Lesser General Public License as published by the Free Software Foundation; either
//   version 2.1 of the License or any later version.  EJDB is distributed in the hope
//   that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
//   License for more details.
//   You should have received a copy of the GNU Lesser General Public License along with EJDB;
//   if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
//   Boston, MA 02111-1307 USA.
// ============================================================================================
using System;



namespace Ejdb.SON {

	/** <summary> BSON types according to the bsonspec (http://bsonspec.org/)</summary> */ 

	public enum BSONType : byte {
		BSON_EOO = 0,
		BSON_DOUBLE = 1,
		BSON_STRING = 2,
		BSON_OBJECT = 3,
		BSON_ARRAY = 4,
		BSON_BINDATA = 5,
		BSON_UNDEFINED = 6,
		BSON_OID = 7,
		BSON_BOOL = 8,
		BSON_DATE = 9,
		BSON_NULL = 10,
		BSON_REGEX = 11,
		BSON_DBREF = 12, /**< Deprecated. */
		BSON_CODE = 13,
		BSON_SYMBOL = 14,
		BSON_CODEWSCOPE = 15,
		BSON_INT = 16,
		BSON_TIMESTAMP = 17,
		BSON_LONG = 18	
	}
}

