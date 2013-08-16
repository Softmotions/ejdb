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
using System.Text;
using System.Runtime.InteropServices;

namespace Ejdb.Utils {

	public class Native {
		static Native() {
		}

		public static IntPtr NativeUtf8FromString(string managedString) {
			int len = Encoding.UTF8.GetByteCount(managedString);
			byte[] buffer = new byte[len + 1];
			Encoding.UTF8.GetBytes(managedString, 0, managedString.Length, buffer, 0);
			IntPtr nativeUtf8 = Marshal.AllocHGlobal(buffer.Length);
			Marshal.Copy(buffer, 0, nativeUtf8, buffer.Length);
			return nativeUtf8;
		}

		public static string StringFromNativeUtf8(IntPtr nativeUtf8) {
			int len = 0;
			for (; Marshal.ReadByte(nativeUtf8, len) != 0; ++len) {
			}
			if (len == 0) {
				return string.Empty;
			}
			byte[] buffer = new byte[len];
			Marshal.Copy(nativeUtf8, buffer, 0, buffer.Length);
			return Encoding.UTF8.GetString(buffer);
		}
	}
}

