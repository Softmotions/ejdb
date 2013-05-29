/**************************************************************************************************
 *  Python API for EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
 *
 *  This file is part of EJDB.
 *  EJDB is free software; you can redistribute it and/or modify it under the terms of
 *  the GNU Lesser General Public License as published by the Free Software Foundation; either
 *  version 2.1 of the License or any later version.  EJDB is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *  You should have received a copy of the GNU Lesser General Public License along with EJDB;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA.
 *************************************************************************************************/

#ifndef PYEJDB_H
#define	PYEJDB_H

#include <Python.h>
#include <tcejdb/ejdb_private.h>

#if PY_MAJOR_VERSION >= 3
#define BYTES_FORMAT_STRING "y#"
#else
#define BYTES_FORMAT_STRING "s#"
#endif


#if SIZEOF_SIZE_T > SIZEOF_INT
#define TK_PY_SIZE_T_BIGGER_THAN_INT
#define TK_PY_MAX_LEN ((Py_ssize_t)INT_MAX)
#endif

/* Error */
static PyObject *Error;

/* PyUnicode to char * */
char* PyUnicode_AsString(PyObject *unicode) {
    PyObject *tmp;
    char *result;
    tmp = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(unicode),
            PyUnicode_GET_SIZE(unicode), NULL);
    if (!tmp) {
        return NULL;
    }
    result = PyBytes_AS_STRING(tmp);
    Py_DECREF(tmp);
    return result;
}

char* PyUnicode_Object_AsString(PyObject *obj) {
    if (PyUnicode_Check(obj)) {
        return PyUnicode_AsString(obj);
    }
    if (PyBytes_Check(obj)) {
        return PyBytes_AS_STRING(obj);
    }
    PyErr_SetString(PyExc_TypeError, "a bytes or unicode object is required");
    return NULL;
}

#if PY_MAJOR_VERSION >= 3
#define PyString_AsString PyUnicode_Object_AsString
#define PyInt_FromLong PyLong_FromLong
#endif

int PyModule_AddType(PyObject *module, const char *name, PyTypeObject *type) {
    Py_INCREF(type);
    if (PyModule_AddObject(module, name, (PyObject *) type)) {
        Py_DECREF(type);
        return -1;
    }
    return 0;
}

int check_py_ssize_t_len(Py_ssize_t len, PyObject *obj) {
#ifdef TK_PY_SIZE_T_BIGGER_THAN_INT
    if (len > TK_PY_MAX_LEN) {
        PyErr_Format(PyExc_OverflowError, "%s is too large",
                Py_TYPE(obj)->tp_name);
        return -1;
    }
#endif
    return 0;
}

/* convert a bytes object to a void ptr */
int bytes_to_void(PyObject *pyvalue, void **value, int *value_len) {
    char *tmp;
    Py_ssize_t tmp_len;
    if (PyBytes_AsStringAndSize(pyvalue, &tmp, &tmp_len)) {
        return -1;
    }
    if (check_py_ssize_t_len(tmp_len, pyvalue)) {
        return -1;
    }
    *value = (void *) tmp;
    *value_len = (int) tmp_len;
    return 0;
}

PyObject* void_to_bytes(const void *value, int value_size) {
    return PyBytes_FromStringAndSize((char *) value, (Py_ssize_t) value_size);
}

/* error handling utils */
PyObject* set_error(PyObject *type, const char *message) {
    PyErr_SetString(type, message);
    return NULL;
}

#endif	/* PYEJDB_H */

