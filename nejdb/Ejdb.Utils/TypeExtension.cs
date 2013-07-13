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
using System.Runtime.CompilerServices;

namespace Ejdb.Utils {

	/// <summary>
	/// Check if specified type is anonymous.
	/// </summary>
	public static class TypeExtension {
		public static bool IsAnonymousType(this Type type) {
			bool hasCompilerGeneratedAttribute = (type.GetCustomAttributes(typeof(CompilerGeneratedAttribute), false).Length > 0);
			bool nameContainsAnonymousType = (type.FullName.Contains("AnonType") || type.FullName.Contains("AnonymousType"));
			return (hasCompilerGeneratedAttribute && nameContainsAnonymousType);
		}
	}
}

