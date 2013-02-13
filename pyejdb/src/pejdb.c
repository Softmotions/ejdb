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

static PyObject* EJDB_is_open(PEJDB *self) {
    return PyBool_FromLong(self->ejdb && ejdbisopen(self->ejdb) ? 1L : 0L);
}

static PyObject* EJDB_close(PEJDB *self) {
    if (!ejdbclose(self->ejdb)) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_sync(PEJDB *self) {
    if (!ejdbsyncdb(self->ejdb)) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_dropCollection(PEJDB *self, PyObject *args, PyObject *kwargs) {
    const char *cname;
    PyObject *prune = Py_False;
    static char *kwlist[] = {"cname", "prune", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O:EJDB_dropCollection", kwlist,
            &cname, &prune)) {
        return NULL;
    }
    if (!ejdbrmcoll(self->ejdb, cname, (prune == Py_True))) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_ensureCollection(PEJDB *self, PyObject *args, PyObject *kwargs) {
    const char *cname;
    int cachedrecords = 0, records = 0;
    PyObject *compressed = Py_False, *large = Py_False;

    static char *kwlist[] = {"cname", "cachedrecords", "compressed", "large", "records", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|iOOi:EJDB_ensureCollection", kwlist,
            &cname, &cachedrecords, &compressed, &large, &records)) {
        return NULL;
    }
    EJCOLLOPTS jcopts = {NULL};
    jcopts.cachedrecords = (cachedrecords > 0) ? cachedrecords : 0;
    jcopts.compressed = (compressed == Py_True);
    jcopts.large = (large == Py_True);
    jcopts.records = (records > 0) ? records : 0;
    EJCOLL *coll = ejdbcreatecoll(self->ejdb, cname, &jcopts);
    if (!coll) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_save(PEJDB *self, PyObject *args, PyObject *kwargs) {
    const char *cname;
    PyObject *merge = Py_False;
    void *bsonbuf = NULL;
    int bsonbufz;
    PyObject *bsonbufpy;
    EJCOLL *ejcoll;
    bson_oid_t oid;
    bson bsonval;
    bool bret = false;
    static char *kwlist[] = {"cname", "bson", "merge", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|O:EJDB_save", kwlist,
            &cname, &bsonbufpy, &merge)) {
        return NULL;
    }
    if (bytes_to_void(bsonbufpy, &bsonbuf, &bsonbufz)) {
        return NULL;
    }
    if (!PyBool_Check(merge)) {
        return set_error(PyExc_TypeError, "'merge' must be an instance of boolean type");
    }
    Py_BEGIN_ALLOW_THREADS
    ejcoll = ejdbcreatecoll(self->ejdb, cname, NULL);
    Py_END_ALLOW_THREADS
    if (!ejcoll) {
        return set_ejdb_error(self->ejdb);
    }
    bson_init_finished_data(&bsonval, bsonbuf);
    bsonval.flags |= BSON_FLAG_STACK_ALLOCATED;
    Py_BEGIN_ALLOW_THREADS
    bret = ejdbsavebson2(ejcoll, &bsonval, &oid, (merge == Py_True));
    Py_END_ALLOW_THREADS
    bson_destroy(&bsonval);
    if (!bret) {
        return set_ejdb_error(self->ejdb);
    }
    char xoid[25];
    bson_oid_to_string(&oid, xoid);
    return PyUnicode_FromString(xoid);
}

static PyObject* EJDB_load(PEJDB *self, PyObject *args) {
    const char *cname;
    const char *soid;
    if (!PyArg_ParseTuple(args, "ss:EJDB_load", &cname, &soid)) {
        return NULL;
    }
    if (!ejdbisvalidoidstr(soid)) {
        return set_error(PyExc_ValueError, "Invalid OID");
    }
    EJCOLL *coll = ejdbgetcoll(self->ejdb, cname);
    if (!coll) {
        Py_RETURN_NONE;
    }
    bson_oid_t oid;
    bson_oid_from_string(&oid, soid);
    bson *doc;
    Py_BEGIN_ALLOW_THREADS
    doc = ejdbloadbson(coll, &oid);
    Py_END_ALLOW_THREADS
    if (!doc) {
        if (ejdbecode(self->ejdb) != TCESUCCESS) {
            return set_ejdb_error(self->ejdb);
        } else {
            Py_RETURN_NONE;
        }
    }
    PyObject *docbytes = PyBytes_FromStringAndSize(bson_data(doc), bson_size(doc));
    bson_del(doc);
    return docbytes;
}

static PyObject* EJDB_remove(PEJDB *self, PyObject *args) {
    const char *cname;
    const char *soid;
    bool bret = false;
    if (!PyArg_ParseTuple(args, "ss:EJDB_remove", &cname, &soid)) {
        return NULL;
    }
    if (!ejdbisvalidoidstr(soid)) {
        return set_error(PyExc_ValueError, "Invalid OID");
    }
    EJCOLL *coll = ejdbgetcoll(self->ejdb, cname);
    if (!coll) {
        Py_RETURN_NONE;
    }
    bson_oid_t oid;
    bson_oid_from_string(&oid, soid);
    Py_BEGIN_ALLOW_THREADS
    bret = ejdbrmbson(coll, &oid);
    Py_END_ALLOW_THREADS
    if (!bret) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_find(PEJDB *self, PyObject *args) {
    const char *cname;
    PyObject *qbsbufpy, *orlistpy, *hbufpy;
    void *qbsbuf, *hbsbuf;
    int qbsbufsz, hbsbufsz;
    bool err = false;
    Py_ssize_t orsz = 0;
    bson oqarrstack[8]; //max 8 $or bsons on stack
    bson *oqarr = NULL;
    bson qbson = {NULL};
    bson hbson = {NULL};
    EJQ *q = NULL;
    int qflags = 0;
    TCLIST *qres = NULL;
    EJCOLL *coll = NULL;
    PyObject *pyret = NULL;
    uint32_t count = 0;

    //cname, qobj, qobj, orarr, hints, qflags
    if (!PyArg_ParseTuple(args, "sOOOi:EJDB_find",
            &cname, &qbsbufpy, &orlistpy, &hbufpy, &qflags)) {
        return NULL;
    }
    if (bytes_to_void(qbsbufpy, &qbsbuf, &qbsbufsz)) {
        return NULL;
    }
    if (bytes_to_void(hbufpy, &hbsbuf, &hbsbufsz)) {
        return NULL;
    }
    if (!PyList_Check(orlistpy)) {
        return set_error(PyExc_TypeError, "'orarr' must be an list object");
    }
    orsz =  PyList_GET_SIZE(orlistpy);
    oqarr = ((orsz <= 8) ? oqarrstack : (bson*) malloc(orsz * sizeof (bson)));
    if (!oqarr) {
        return PyErr_NoMemory();
    }
    for (Py_ssize_t i = 0; i < orsz; ++i) {
        void *bsdata;
        int bsdatasz;
        PyObject *orpy = PyList_GET_ITEM(orlistpy, i);
        if (bytes_to_void(orpy, &bsdata, &bsdatasz)) {
            err = true;
            orsz = (i > 0) ? (i - 1) : 0;
            goto finish;
        }
        bson_init_finished_data(&oqarr[i], bsdata);
        oqarr[i].flags |= BSON_FLAG_STACK_ALLOCATED; //prevent bson data to be freed in bson_destroy
    }
    bson_init_finished_data(&qbson, qbsbuf);
    qbson.flags |= BSON_FLAG_STACK_ALLOCATED;
    bson_init_finished_data(&hbson, hbsbuf);
    hbson.flags |= BSON_FLAG_STACK_ALLOCATED;

    //create the query
    q = ejdbcreatequery(self->ejdb, &qbson, orsz > 0 ? oqarr : NULL, orsz, &hbson);
    if (!q) {
        err = true;
        set_ejdb_error(self->ejdb);
        goto finish;
    }
    coll = ejdbgetcoll(self->ejdb, cname);
    if (!coll) {
        bson_iterator it;
        //If we are in $upsert mode a new collection will be created
        if (bson_find(&it, &qbson, "$upsert") == BSON_OBJECT) {
            Py_BEGIN_ALLOW_THREADS
            coll = ejdbcreatecoll(self->ejdb, cname, NULL);
            Py_END_ALLOW_THREADS
            if (!coll) {
                err = true;
                set_ejdb_error(self->ejdb);
                goto finish;
            }
        }
    }

    if (!coll) { //No collection -> no results
        qres = (qflags & JBQRYCOUNT) ? NULL : tclistnew2(1); //empty results
    } else {
        Py_BEGIN_ALLOW_THREADS
        qres = ejdbqryexecute(coll, q, &count, qflags, NULL);
        Py_END_ALLOW_THREADS
        if (ejdbecode(self->ejdb) != TCESUCCESS) {
            err = true;
            set_ejdb_error(self->ejdb);
            goto finish;
        }
    }

    if (qres) {
        pyret = PDBCursor_tp_new(&DBCursorType, (PyObject *) self, qres);
        if (!pyret) {
            err = true;
            goto finish;
        }
    }

finish:
    for (Py_ssize_t i = 0; i < orsz; ++i) {
        bson_destroy(&oqarr[i]);
    }
    if (oqarr && oqarr != oqarrstack) {
        free(oqarr);
    }
    bson_destroy(&qbson);
    bson_destroy(&hbson);
    if (q) {
        ejdbquerydel(q);
    }
    if (err) {
        Py_XDECREF(pyret);
        return NULL;
    } else {
        return (pyret) ? pyret : PyLong_FromUnsignedLong(count);
    }
}

/* EJDBType.tp_methods */
static PyMethodDef EJDB_tp_methods[] = {
    {"open", (PyCFunction) EJDB_open, METH_VARARGS, NULL},
    {"isopen", (PyCFunction) EJDB_is_open, METH_NOARGS, NULL},
    {"close", (PyCFunction) EJDB_close, METH_NOARGS, NULL},
    {"save", (PyCFunction) EJDB_save, METH_VARARGS | METH_KEYWORDS, NULL},
    {"load", (PyCFunction) EJDB_load, METH_VARARGS, NULL},
    {"remove", (PyCFunction) EJDB_remove, METH_VARARGS, NULL},
    {"sync", (PyCFunction) EJDB_sync, METH_NOARGS, NULL},
    {"find", (PyCFunction) EJDB_find, METH_VARARGS, NULL},
    {"ensureCollection", (PyCFunction) EJDB_ensureCollection, METH_VARARGS | METH_KEYWORDS, NULL},
    {"dropCollection", (PyCFunction) EJDB_dropCollection, METH_VARARGS | METH_KEYWORDS, NULL},
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
