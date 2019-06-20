.. _cmd:
.. _iexp:

Import/Export database data
===========================

Export or import of the database data can be done by using `ejdbcommand` API function in language
binding of your choice.

C API
-----

.. c:function:: bson* ejdbcommand(EJDB *jb, bson *cmdbson)

    Execute the ejdb database command

    :param EJDB* jb: Pointer to the database object
    :param bson* cmdbson: :ref:`Command specification <export_cmd_spec>`
    :return: Allocated :ref:`command response <import_cmd_response>` BSON object.
             Caller should call `bson_del()` on it.

.. _export_cmd_spec:

Export command spec
-------------------

.. code-block:: js

    {
        'export': {
            path: '<Export files target directory>',
            cnames: [], //List of collection names to export
            mode: 0 //Export mode
        }
    }

Export `mode` is an integer flag describes the following options:

* `0`: Database collections will be exported as BSON documents (Default).
* `JBJSONEXPORT`: Database collections will be exported as JSON files.


Import command spec
-------------------

.. code-block:: js

    {
        'import': {
            path: '<Import files source directory>'
            cnames: [], //List of collection names to import
            mode: 0 //Import mode
        }
    }

Import `mode` is an integer flag describes the following options:

* `0` or `JBIMPORTUPDATE`: Update existing collection entries with imported ones.
                           Missing collections will be created.

* `JBIMPORTREPLACE`: Recreate all collections and replace all collection data with imported entries.
                     Missing collections will be created.


.. _cmd_response:
.. _import_cmd_response:
.. _export_cmd_response:

Command response
----------------

.. code-block:: js

    {
        log: '<Command execution diagnostic log>',
        error: '<Error message>',
        errorCode: 0 //Error code number
    }

