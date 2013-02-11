#include "pyejdb.h"

PyObject* set_ejdb_error(EJDB *ejdb) {
    int ecode;
    ecode = ejdbecode(ejdb);
    return set_error(Error, ejdberrmsg(ecode));
}

/* EJDBType.tp_dealloc */
static void EJDB_tp_dealloc(PEJDB *self) {
    if (self->ejdb) {
        ejdbdel(self->ejdb);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/* EJDBType.tp_new */
static PyObject* EJDB_tp_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    PEJDB *self = (PEJDB*) type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    /* self->e */
    self->ejdb = ejdbnew();
    if (!self->ejdb) {
        set_error(Error, "could not create EJDB");
        Py_DECREF(self);
        return NULL;
    }
    return (PyObject *) self;
}

static PyObject* EJDB_open(PEJDB *self, PyObject *args) {
    const char *path;
    int mode;
    if (!PyArg_ParseTuple(args, "si:open", &path, &mode)) {
        return NULL;
    }
    if (!ejdbopen(self->ejdb, path, mode)) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_close(PEJDB *self) {
    if (!ejdbclose(self->ejdb)) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_save(PEJDB *self, PyObject *args, PyObject *kwargs) {
    const char *cname;
    PyObject *merge = Py_False;
    Py_buffer *bsonbuff;
    EJCOLL *ejcoll;
    bson_oid_t oid;
    bson bsonval;
    bool ret = false;
    static char *kwlist[] = {"cname", "bson", "merge", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss*|O:EJDB_save", kwlist,
            &cname, &bsonbuff, &merge)) {
        return NULL;
    }
    if (!PyBool_Check(merge)) {
        PyBuffer_Release(bsonbuff);
        return set_error(PyExc_TypeError, "'merge' must be an instance of boolean type");
    }
    Py_BEGIN_ALLOW_THREADS
    ejcoll = ejdbcreatecoll(self->ejdb, cname, NULL);
    Py_END_ALLOW_THREADS
    if (!ejcoll) {
        PyBuffer_Release(bsonbuff);
        return set_ejdb_error(self->ejdb);
    }
    bson_init_finished_data(&bsonval, bsonbuff->buf);
    Py_BEGIN_ALLOW_THREADS
    ret = ejdbsavebson2(ejcoll, &bsonval, &oid, (merge == Py_True));
    Py_END_ALLOW_THREADS
    if (!ret) {
        PyBuffer_Release(bsonbuff);
        return set_ejdb_error(self->ejdb);
    }
    PyBuffer_Release(bsonbuff);
    bsonval.data = NULL;
    bson_destroy(&bsonval);

    //todo return OID
    //PyString_FromString()
    Py_RETURN_NONE;
}


//EJDB_EXPORT EJCOLL* ejdbcreatecoll(EJDB *jb, const char *colname, EJCOLLOPTS *opts);
//EJDB_EXPORT bool ejdbsavebson2(EJCOLL *jcoll, bson *bs, bson_oid_t *oid, bool merge);


/* EJDBType.tp_methods */
static PyMethodDef EJDB_tp_methods[] = {
    {"open", (PyCFunction) EJDB_open, METH_VARARGS, NULL},
    {"close", (PyCFunction) EJDB_close, METH_NOARGS, NULL},
    {"save", (PyCFunction) EJDB_save, METH_VARARGS | METH_KEYWORDS, NULL},
    {NULL}
};


static PyTypeObject EJDBType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_pyejdb.EJDB",                           /*tp_name*/
    sizeof (PEJDB),                           /*tp_basicsize*/
    0,                                        /*tp_itemsize*/
    (destructor) EJDB_tp_dealloc,             /*tp_dealloc*/
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    0,                                        /*tp_doc*/
    0,                                        /*tp_traverse*/
    0,                                        /*tp_clear*/
    0,                                        /*tp_richcompare*/
    0,                                        /*tp_weaklistoffset*/
    0,                                        /*tp_iter*/
    0,                                        /*tp_iternext*/
    EJDB_tp_methods,                          /*tp_methods*/
    0,                                        /*tp_members*/
    0,                                        /*tp_getsets*/
    0,                                        /*tp_base*/
    0,                                        /*tp_dict*/
    0,                                        /*tp_descr_get*/
    0,                                        /*tp_descr_set*/
    0,                                        /*tp_dictoffset*/
    0,                                        /*tp_init*/
    0,                                        /*tp_alloc*/
    EJDB_tp_new,                              /*tp_new*/
};
