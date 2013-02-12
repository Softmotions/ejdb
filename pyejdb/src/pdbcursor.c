#include "pyejdb.h"

static PyObject* PDBCursor_tp_new(PyTypeObject *type, PyObject *db, TCLIST *res) {
    PDBCursor *self = (PDBCursor *) type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    Py_INCREF(db);
    self->db = db;
    self->res = res;
    self->pos = 0;
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
    if (self->res) {
        tclistdel(self->res);
        self->res = NULL;
    }
    PDBCursor_tp_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}
