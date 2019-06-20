.. _mongodb:

Mongodb compatibility
=====================

.. contents::

Logical operators
-----------------

.. list-table::
    :widths: 15 10 30
    :header-rows: 1

    *   - Operator
        - Status
        - Notes
    *   - $or
        - Y
        -
    *   - $and
        - Y
        -
    *   - $not
        - Y
        -
    *   - $nor
        - N
        -


Element level operators
-----------------------

.. list-table::
    :widths: 15 10 30
    :header-rows: 1

    *   - Operator
        - Status
        - Notes
    *   - $exists
        - Y
        -
    *   - $mod
        - N
        -
    *   - $bit
        - N
        -
    *   - $type
        - N
        -
    *   - $where
        - N
        - JavaScript. EJDB library does not embed a JS engine.
          It may be hard to include this type of engine into embedded DB.
    *   - $regex
        - YN
        - Regexps are supported in queries.


Geospartial operators
---------------------

.. list-table::
    :widths: 15 10 30
    :header-rows: 1

    *   - Operator
        - Status
        - Notes
    *   - $geoWithin, $geoIntersects, $near, $nearSphere
        - N
        -


Array operators
---------------

.. list-table::
    :widths: 15 10 30
    :header-rows: 1

    *   - Operator
        - Status
        - Notes
    *   - $elemMatch
        - Y
        -
    *   - $(projection)
        - Y
        - :ref:`Our implementation <$(projection)>` overcomes the following mongodb restriction: "Only one array field can appear in the query document"
    *   - $size
        - N
        -

Update operators
----------------

.. list-table::
    :widths: 15 10 30
    :header-rows: 1

    *   - Operator
        - Status
        - Notes
    *   - $set
        - Y
        -
    *   - $unset
        - Y
        -
    *   - $(query)
        - Y
        -
    *   - $addToSet, $pull
        - Y
        - The EJDB specific :ref:`$addToSetAll <$addToSetAll>`, :ref:`$pullAll <$pullAll>` are also implemented.
    *   - $setOnInsert
        - YN
        - Special :ref:`$upsert` operator is used. Behaviour is a bit different
    *   - $each
        - YN
        - :ref:`$addToSetAll <$addToSetAll>` is used instead
    *   - $rename
        - Y
        -
    *   - $slice
        - Y
        -
    *   - $sort
        - N
        -
    *   - $push
        - Y
        - :ref:`$pushAll <$pushAll>` is the batch version of :ref:`$push <$push>` operator.

Various operators
-----------------

.. list-table::
    :widths: 15 10 30
    :header-rows: 1

    *   - Operator
        - Status
        - Notes
    *   - $orderby
        - Y
        -
    *   - $fields
        - Y
        -
    *   - $maxScan
        - YN
        - EJDB :ref:`$max` operator is used
    *   - $hint
        - N
        - Explicit index selection hints are reserved. But they can not be used in the current EJDB version
    *   - $max, $min, $returnKey, $comment
        - N
        -
    *   - $explain
        - NN
        - Used simple logging facility to trace query execution
    *   - $showDiskLoc
        - NN
        - Mongodb implementation specific feature



EJDB specific query operators
-----------------------------

.. list-table::
    :widths: 30 70
    :header-rows: 1

    *   - Operator
        - Notes
    *   - :ref:`$dropall`
        - In-place record removal operation
    *   - :ref:`$bt <$bt>`
        - Matching between a numbers (as in SQL)
    *   - :ref:`$do <$do>`
        - Perform an action, for example :ref:`In-query join of two collections <joins>`
    *   - :ref:`$icase <$icase>`
        - Case insensitive string matching
    *   - :ref:`$begin <$begin>`
        - String prefix matching. Available collection indexes may be in use
    *   - :ref:`$strand <$strand>` , :ref:`$stror <$stror>`
        - String tokens/String array matches all/any token in specified array
    *   - :ref:`$addToSetAll <$addToSetAll>` , :ref:`$pullAll <$pullAll>`
        - Batch versions of :ref:`$addToSet <$addToSet>` , :ref:`$pull <$pull>`

