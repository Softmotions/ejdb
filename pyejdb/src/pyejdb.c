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

#include "pyejdb.h"

/* PEJDB */
typedef struct {
    PyObject_HEAD
    EJDB *ejdb;
} PEJDB;

/* Cursor */
typedef struct {
    PyObject_HEAD
    PyObject *db;
    TCLIST *res;
} PDBCursor;

#define PYEJDBTXBEGIN 1
#define PYEJDBTXABORT 2
#define PYEJDBTXCOMMIT 3
#define PYEJDBTXSTATUS 4


#include "pdbcursor.c"
#include "pejdb.c"

PyDoc_STRVAR(ejdb_version_doc, "version() -> str\n\nReturns the version string of the underlying EJDB library.");

static PyObject* ejdb_version(PyObject *module) {
    return PyUnicode_FromString(tcversion);
}

/* pyejdb.m_methods */
static PyMethodDef pyejdb_m_methods[] = {
    {"ejdb_version", (PyCFunction) ejdb_version, METH_NOARGS, ejdb_version_doc},
    {NULL} /* Sentinel */
};


#if PY_MAJOR_VERSION >= 3
PyDoc_STRVAR(ejdb_m_doc, "EJDB http://ejdb.org");
#define INITERROR return NULL

/* pyejdb_module */
static PyModuleDef pyejdb_module = {
    PyModuleDef_HEAD_INIT,
    "_pyejdb",          /*m_name*/
    ejdb_m_doc,         /*m_doc*/
    -1,                 /*m_size*/
    pyejdb_m_methods,   /*m_methods*/
};
#else
#define INITERROR return
#endif

PyMODINIT_FUNC init_pyejdb(void) {
    PyObject *pyejdb;
    if (PyType_Ready(&EJDBType) ||
            PyType_Ready(&DBCursorType)
            ) {
        INITERROR;
    }

#if PY_MAJOR_VERSION >= 3
    pyejdb = PyModule_Create(&pyejdb_module);
#else
    pyejdb = Py_InitModule("_pyejdb", pyejdb_m_methods);
#endif

    if (!pyejdb) {
        INITERROR;
    }
    Error = PyErr_NewException("_pyejdb.Error", NULL, NULL);
    Py_XINCREF(Error);
    if (!Error || PyModule_AddObject(pyejdb, "Error", Error)) {
        Py_XDECREF(Error);
        goto fail;
    }

    /* adding types and constants */
    if (
            PyModule_AddType(pyejdb, "EJDB", &EJDBType) ||
            PyModule_AddType(pyejdb, "DBCursor", &DBCursorType) ||

            PyModule_AddIntMacro(pyejdb, JBOREADER) ||
            PyModule_AddIntMacro(pyejdb, JBOWRITER) ||
            PyModule_AddIntMacro(pyejdb, JBOCREAT) ||
            PyModule_AddIntMacro(pyejdb, JBOTRUNC) ||
            PyModule_AddIntMacro(pyejdb, JBONOLCK) ||
            PyModule_AddIntMacro(pyejdb, JBOLCKNB) ||
            PyModule_AddIntMacro(pyejdb, JBOTSYNC) ||

            PyModule_AddIntMacro(pyejdb, JBIDXDROP) ||
            PyModule_AddIntMacro(pyejdb, JBIDXDROPALL) ||
            PyModule_AddIntMacro(pyejdb, JBIDXOP) ||
            PyModule_AddIntMacro(pyejdb, JBIDXREBLD) ||
            PyModule_AddIntMacro(pyejdb, JBIDXNUM) ||
            PyModule_AddIntMacro(pyejdb, JBIDXSTR) ||
            PyModule_AddIntMacro(pyejdb, JBIDXARR) ||
            PyModule_AddIntMacro(pyejdb, JBIDXISTR) ||

            PyModule_AddIntMacro(pyejdb, PYEJDBTXBEGIN) ||
            PyModule_AddIntMacro(pyejdb, PYEJDBTXABORT) ||
            PyModule_AddIntMacro(pyejdb, PYEJDBTXCOMMIT) ||
            PyModule_AddIntMacro(pyejdb, PYEJDBTXSTATUS) ||

            PyModule_AddIntMacro(pyejdb, JBQRYCOUNT)
            ) {

        goto fail;
    }

#if PY_MAJOR_VERSION >= 3
    return pyejdb;
#else
    return;
#endif

fail:
    Py_DECREF(pyejdb);
    INITERROR;
}

PyMODINIT_FUNC PyInit__pyejdb(void) {
    return init_pyejdb();
}

