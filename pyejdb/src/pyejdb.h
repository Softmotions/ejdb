/*
 * File:   pyejdb.h
 * Author: adam
 *
 * Created on February 7, 2013, 11:37 PM
 */

#ifndef PYEJDB_H
#define	PYEJDB_H

#include <Python.h>
#include <tcejdb/ejdb.h>

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
#define PyString_FromString PyUnicode_FromString
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

/* error handling utils */
PyObject* set_error(PyObject *type, const char *message) {
    PyErr_SetString(type, message);
    return NULL;
}

#endif	/* PYEJDB_H */

