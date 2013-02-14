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
    bool bres = false;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O:EJDB_dropCollection", kwlist,
            &cname, &prune)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    bres = ejdbrmcoll(self->ejdb, cname, (prune == Py_True));
    Py_END_ALLOW_THREADS
    if (!bres) {
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
    EJCOLL *coll;
    Py_BEGIN_ALLOW_THREADS
    coll = ejdbcreatecoll(self->ejdb, cname, &jcopts);
    Py_END_ALLOW_THREADS
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

static PyObject* EJDB_setIndex(PEJDB *self, PyObject *args) {
    const char *cname;
    const char *path;
    int flags;
    bool bret;
    if (!PyArg_ParseTuple(args, "ssi:EJDB_setIndex", &cname, &path, &flags)) {
        return NULL;
    }
    EJCOLL *coll = ejdbcreatecoll(self->ejdb, cname, NULL);
    if (!coll) {
        return set_ejdb_error(self->ejdb);
    }
    Py_BEGIN_ALLOW_THREADS
    bret = ejdbsetindex(coll, path, flags);
    Py_END_ALLOW_THREADS
    if (!bret) {
        return set_ejdb_error(self->ejdb);
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_txcontrol(PEJDB *self, PyObject *args) {
    const char *cname;
    int cmd;
    bool bret;
    if (!PyArg_ParseTuple(args, "si:EJDB_txcontrol", &cname, &cmd)) {
        return NULL;
    }
    EJCOLL *coll = ejdbcreatecoll(self->ejdb, cname, NULL);
    if (!coll) {
        return set_ejdb_error(self->ejdb);
    }

    switch (cmd) {
        case PYEJDBTXBEGIN:
        {
            bret = ejdbtranbegin(coll);
            if (!bret) {
                return set_ejdb_error(self->ejdb);
            }
            break;
        }
        case PYEJDBTXABORT:
        {
            bret = ejdbtranabort(coll);
            if (!bret) {
                return set_ejdb_error(self->ejdb);
            }
            break;
        }
        case PYEJDBTXCOMMIT:
        {
            Py_BEGIN_ALLOW_THREADS
            bret = ejdbtrancommit(coll);
            Py_END_ALLOW_THREADS
            if (!bret) {
                return set_ejdb_error(self->ejdb);
            }
            break;
        }
        case PYEJDBTXSTATUS:
        {
            bool txactive = false;
            bret = ejdbtranstatus(coll, &txactive);
            if (!bret) {
                return set_ejdb_error(self->ejdb);
            }
            return PyBool_FromLong(txactive);
            break;
        }
    }
    Py_RETURN_NONE;
}

static PyObject* EJDB_dbmeta(PEJDB *self, PyObject *args) {
    PyObject *val = NULL;
    PyObject *carr = PyList_New(0);
    PyObject *pycont = PyDict_New();
    if (!pycont || !carr) {
        goto fail;
    }
    TCLIST *cols = ejdbgetcolls(self->ejdb);
    if (!cols) {
        set_ejdb_error(self->ejdb);
        goto fail;
    }
    for (int i = 0; i < TCLISTNUM(cols); ++i) {

        PyObject *pycoll = NULL;
        PyObject *pyindexes = NULL;
        PyObject *pyopts = NULL;
        PyObject *pyimeta = NULL;

        EJCOLL *coll = (EJCOLL*) TCLISTVALPTR(cols, i);
        pycoll = PyDict_New();
        if (!pycoll) {
            goto ffail;
        }
        val = PyUnicode_FromStringAndSize(coll->cname, coll->cnamesz);
        PyDict_SetItemString(pycoll, "name", val);
        Py_XDECREF(val);

        val = PyUnicode_FromString(coll->tdb->hdb->path);
        PyDict_SetItemString(pycoll, "file", val);
        Py_XDECREF(val);

        val = PyLong_FromUnsignedLongLong(coll->tdb->hdb->rnum);
        PyDict_SetItemString(pycoll, "records", val);
        Py_XDECREF(val);

        pyopts = PyDict_New();
        if (!pyopts) {
            goto ffail;
        }
        val = PyLong_FromUnsignedLongLong(coll->tdb->hdb->bnum);
        PyDict_SetItemString(pyopts, "buckets", val);
        Py_XDECREF(val);

        val = PyLong_FromUnsignedLong(coll->tdb->hdb->rcnum);
        PyDict_SetItemString(pyopts, "cachedrecords", val);
        Py_XDECREF(val);

        val = PyBool_FromLong(coll->tdb->opts & TDBTLARGE);
        PyDict_SetItemString(pyopts, "large", val);
        Py_XDECREF(val);

        val = PyBool_FromLong(coll->tdb->opts & TDBTDEFLATE);
        PyDict_SetItemString(pyopts, "compressed", val);
        Py_XDECREF(val);

        PyDict_SetItemString(pycoll, "options", pyopts);
        Py_XDECREF(pyopts);

        pyindexes = PyList_New(0);
        if (!pyindexes) {
            goto ffail;
        }
        for (int j = 0; j < coll->tdb->inum; ++j) {
            TDBIDX *idx = (coll->tdb->idxs + j);
            assert(idx);
            if (idx->type != TDBITLEXICAL && idx->type != TDBITDECIMAL && idx->type != TDBITTOKEN) {
                continue;
            }
            pyimeta = PyDict_New();
            if (!pyimeta) {
                goto ffail;
            }
            val = PyUnicode_FromString(idx->name + 1);
            PyDict_SetItemString(pyimeta, "field", val);
            Py_XDECREF(val);

            val = PyUnicode_FromString(idx->name);
            PyDict_SetItemString(pyimeta, "iname", val);
            Py_XDECREF(val);

            switch (idx->type) {
                case TDBITLEXICAL:
                    val = PyUnicode_FromString("lexical");
                    PyDict_SetItemString(pyimeta, "type", val);
                    Py_XDECREF(val);
                    break;
                case TDBITDECIMAL:
                    val = PyUnicode_FromString("decimal");
                    PyDict_SetItemString(pyimeta, "type", val);
                    Py_XDECREF(val);
                    break;
                case TDBITTOKEN:
                    val = PyUnicode_FromString("token");
                    PyDict_SetItemString(pyimeta, "type", val);
                    Py_XDECREF(val);
                    break;
            }
            TCBDB *idb = (TCBDB*) idx->db;
            if (idb) {
                val = PyLong_FromUnsignedLongLong(idb->rnum);
                PyDict_SetItemString(pyimeta, "records", val);
                Py_XDECREF(val);

                val = PyUnicode_FromString(idb->hdb->path);
                PyDict_SetItemString(pyimeta, "file", val);
                Py_XDECREF(val);

            }
            PyList_Append(pyindexes, pyimeta);
        }
        PyDict_SetItemString(pycoll, "indexes", pyindexes);
        Py_XDECREF(pyindexes);

        PyList_Append(carr, pycoll);
        continue;

ffail:
        Py_XDECREF(pycoll);
        Py_XDECREF(pyindexes);
        Py_XDECREF(pyopts);
        Py_XDECREF(pyimeta);
        goto fail;
    }

    val = PyUnicode_FromString(self->ejdb->metadb->hdb->path);
    PyDict_SetItemString(pycont, "file", val);
    Py_XDECREF(val);

    PyDict_SetItemString(pycont, "collections", carr);
    Py_XDECREF(carr);

    return pycont;
fail:
    Py_XDECREF(carr);
    Py_XDECREF(pycont);
    return NULL;
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
    {"setIndex", (PyCFunction) EJDB_setIndex, METH_VARARGS, NULL},
    {"txcontrol", (PyCFunction) EJDB_txcontrol, METH_VARARGS, NULL},
    {"dbmeta", (PyCFunction) EJDB_dbmeta, METH_VARARGS, NULL},
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
