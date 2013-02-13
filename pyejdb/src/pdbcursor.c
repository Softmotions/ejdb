#include "pyejdb.h"

static PyObject* PDBCursor_close(PDBCursor *self);

static PyObject* PDBCursor_tp_new(PyTypeObject *type, PyObject *db, TCLIST *res) {
    PDBCursor *self = (PDBCursor *) type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    Py_INCREF(db);
    self->db = db;
    self->res = res;
    return (PyObject *) self;
}

static int PDBCursor_tp_traverse(PDBCursor *self, visitproc visit, void *arg) {
    Py_VISIT(self->db);
    return 0;
}

static int PDBCursor_tp_clear(PDBCursor *self) {
    Py_CLEAR(self->db);
    return 0;
}

static void PDBCursor_tp_dealloc(PDBCursor *self) {
    PDBCursor_close(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject* PDBCursor_get(PDBCursor *self, PyObject *args) {
    int idx, bsdatasz;
    void *bsdata;
    if (!PyArg_ParseTuple(args, "i:PDBCursor_get", &idx)) {
        return NULL;
    }
    if (!self->res) {
        return set_error(Error, "Cursor closed");
    }
    if (idx < 0 || idx >= TCLISTNUM(self->res)) {
        return set_error(PyExc_IndexError, "Invalid cursor index");
    }
    TCLISTVAL(bsdata, self->res, idx, bsdatasz);
    return void_to_bytes(bsdata, bsdatasz);
}

static PyObject* PDBCursor_length(PDBCursor *self) {
    if (!self->res) {
        return set_error(Error, "Cursor closed");
    }
    return PyLong_FromLong(TCLISTNUM(self->res));
}

static PyObject* PDBCursor_close(PDBCursor *self) {
    if (self->res) {
        tclistdel(self->res);
        self->res = NULL;
    }
    PDBCursor_tp_clear(self);
    Py_RETURN_NONE;
}

static PyMethodDef PDBCursor_tp_methods[] = {
    {"get", (PyCFunction) PDBCursor_get, METH_VARARGS, NULL},
    {"length", (PyCFunction) PDBCursor_length, METH_NOARGS, NULL},
    {"close", (PyCFunction) PDBCursor_close, METH_NOARGS, NULL},
    {NULL}
};


static PyTypeObject DBCursorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_pyejdb.DBCursor",                       /*tp_name*/
    sizeof (PDBCursor),                       /*tp_basicsize*/
    0,                                        /*tp_itemsize*/
    (destructor) PDBCursor_tp_dealloc,        /*tp_dealloc*/
    0,                                        /*tp_print*/
    0,                                        /*tp_getattr*/
    0,                                        /*tp_setattr*/
    0,                                        /*tp_compare*/
    0,                                        /*tp_repr*/
    0,                                        /*tp_as_number*/
    0,                                        /*tp_as_sequence*/
    0,                                        /*tp_as_mapping*/
    0,                                        /*tp_hash */
    0,                                        /*tp_call*/
    0,                                        /*tp_str*/
    0,                                        /*tp_getattro*/
    0,                                        /*tp_setattro*/
    0,                                        /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,  /*tp_flags*/
    0,                                        /*tp_doc*/
    (traverseproc) PDBCursor_tp_traverse,     /*tp_traverse*/
    (inquiry) PDBCursor_tp_clear,             /*tp_clear*/
    0,                                        /*tp_richcompare*/
    0,                                        /*tp_weaklistoffset*/
    0,                                        /*tp_iter*/
    0,                                        /*tp_iternext*/
    PDBCursor_tp_methods,                     /*tp_methods*/
    0,                                        /*tp_members*/
    0,                                        /*tp_getsets*/
    0,                                        /*tp_base*/
    0,                                        /*tp_dict*/
    0,                                        /*tp_descr_get*/
    0,                                        /*tp_descr_set*/
    0,                                        /*tp_dictoffset*/
    0,                                        /*tp_init*/
    0,                                        /*tp_alloc*/
    0,                                        /*tp_new*/
};
