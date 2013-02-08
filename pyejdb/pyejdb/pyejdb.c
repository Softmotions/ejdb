#include "pyejdb.h"

/* HDB */
typedef struct {
    PyObject_HEAD
    EJDB *ejdb;
} EJDB;

#include "EJDB.c"

PyObject* init_pyejdb() {
    PyObject *pyejdb;
    if (PyType_Ready(&EJDBType)) {
        return NULL;
    }
    return NULL;
}

#if PY_MAJOR_VERSION >= 3

PyMODINIT_FUNC PyInit_cabinet(void) {
    return init_pyejdb();
}
#else

PyMODINIT_FUNC
initcabinet(void) {
    init_pyejdb();
}
#endif

