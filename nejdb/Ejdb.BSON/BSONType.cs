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
namespace Ejdb.BSON {

	/** <summary> BSON types according to the bsonspec (http://bsonspec.org/)</summary> */ 

	public enum BSONType : byte {
		UNKNOWN = 0xfe,
		EOO = 0x00,
		DOUBLE = 0x01,
		STRING = 0x02,
		OBJECT = 0x03,
		ARRAY = 0x04,
		BINDATA = 0x05,
		UNDEFINED = 0x06,
		OID = 0x07,
		BOOL = 0x08,
		DATE = 0x09,
		NULL = 0x0A,
		REGEX = 0x0B,
		DBREF = 0x0C,
		CODE = 0x0D,
		SYMBOL = 0x0E,
		CODEWSCOPE = 0x0F,
		INT = 0x10,
		TIMESTAMP = 0x11,
		LONG = 0x12,
		MAXKEY = 0xFF,
		MINKEY = 0x7F
	}
}

