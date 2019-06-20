.. _ql:

Query language
==============
The form of EJDB queries is inspired by `Mongodb <http://mongodb.org>`_ and follows the same philosophy. In many cases
EJDB queries are fully comparable with mongodb counterparts. Every EJDB query object can be considered as
JSON document which specifies the way of retrieving/updating a set of documents stored in particular database collection.
The query objects can be joined together by `AND` or `OR` logical conjunctions. The way of how to actually specify
queries depends on an API of particular EJDB :ref:`language binding <bindings>`. Anyway we will use an abstract
queries represented as plain JSONs with some showcases where :ref:`EJDB Nodejs CLI API <cli>` is used.

First, we define the following terms: :term:`abstract document` as a metadocument for all
the documents from a particular collection without an actual field values and with all the
possible document fields and their types belonging to any document in the collection.
You can consider `abstract document` as a prototype for all possible documents in the collection.
In the context of an `abstract document` a :term:`fieldpath` is the path consisting of JSON field names traversed
from the document's root to the particular document field.

EJDB aims to be query compatible with mongodb since it would provide an easy
application migration from/to mongodb and adoption of many developers experienced with
mongodb.


.. contents::

Querying
--------

.. _matching:

Simple matching
***************

``{fieldpath1 : value1, fieldpath2 : value2, ...}``

Select all documents which `fieldpath` values are exactly matched to the values provided in the query.

**Example:**

.. code-block:: js

    ejdb> db.addressbook.find({'firstName': 'Andy', age: 39});
    Found 1 records
    { _id: '55352c20a462ec6800000000',
      firstName: 'Andy',
      age: 39 }


As an argument of simple matching query values you can use a regular expressions:

.. code-block:: js

    //Note the regular expression matching
    ejdb> db.addressbook.find({'firstName': /An.*/});
    Found 1 records
    { _id: '55352c20a462ec6800000000',
      firstName: 'Andy',
      age: 39 }

.. _$not:

$not
****

Negation logical operation:


``{fieldpath : {$not : query}}``

Any part of query can be wrapped by `$not` negation operator:

.. code-block:: js

    //The field is not equal to val
    {'fieldpath' : {'$not' : val}}

    //The field is not equal to the provided subquery condition `{...}`
    {'fieldpath' : {'$not' : {...}}}


**Example:**

.. code-block:: js

    //Person's name not begins with 'Andy'
    ejdb> db.persons.find({'name' : {'$not' : {'$begin' : 'Andy'}}});

.. _$nin:

$nin
****

Not in:


``{fieldpath : {$nin : [value1, value2, ...]}}``

Negation of `$in`_ operator.
The field value is not equal to any of provided alternatives.

**Example:**

.. code-block:: js

    ejdb> db.persons.find({'name' : {"$nin" : ['John Travolta', 'Ivanov']}});


.. note::

    Negation operations: `$not` and `$nin` do not use collection indexes
    so they can be slower in comparison with other matching operations.


.. _$in:

$in
****

Field equals to any member in the provided set:


``{fieldpath : {$in : [value1, value2, ...]}``

If the `fieldpath` holds an array, then the `$in` operator selects the documents whose `fieldpath`
holds an array that contains at least one element that matches a value in the set
specified within `$in` array.


.. _$icase:

$icase
******

Case insensitive string matching:


``{fieldpath : {$icase : query}}``

**Example:**
Case insensitive matching within `$in`_ operator:

.. code-block:: js

    ejdb> db.building.find(
        {'name' : {'$icase' : {'$in' : ['théâtre - театр', 'hello world']}}}
    );

In order to perform effective case insensitive queries consider creating `JBIDXISTR` index on fields:

**Nodejs API:**

.. code-block:: js

    ejdb> db.ensureIStringIndex
    [Function] (cname, path, [cb]) Ensure case insensitive String index for JSON field path


**C API:**

.. code-block:: c

    flags = flags | JBIDXISTR;
    EJDB_EXPORT bool ejdbsetindex(EJCOLL *coll, const char *ipath, int flags);


.. _$begin:

$begin
******

Fieldpath starts with the specified prefix:


``{fieldpath : {$begin : prefix}} }``

**Example:**

.. code-block:: js

    //Person's name not begins with 'Andy'
    ejdb> db.persons.find({'name' : {'$not' : {'$begin' : 'Andy'}}});


Simple projections
******************

You can select only specific document fields by providing `$fields` query :ref:`hints <qhints>`:

.. code-block:: js

    ejdb> db.addressbook.find({'firstName': /An.*/}, {$fields: {age:1}});


See the `$fields`_ projection operator.


AND/OR joining of query expressions
***********************************

.. _$or:

OR joined conditions
^^^^^^^^^^^^^^^^^^^^

In order to use the logical `OR` joining of query clauses you have two options:

1. Use the API of EJDB `find()` function, as shown in following nodejs example: (todo link nodejs function description)

**Example:**

.. code-block:: js

    ejdb> db.addressbook.find({}, [{age: 38}, {age: 39}]);

In this example the array of `OR` joined query clauses is passed as the second argument of the `find` function.

2. Use `$or` query operator in the following form:

``{$or: [ query1, query2, ...] }``

`$or` performs a logical `OR` operation on an array of two or more subqueries.

**Example:**

.. code-block:: js

    ejdb> db.addressbook.find({'$or': [{age: 38}, {age: 39}]});


.. _$and:

AND joined conditions
^^^^^^^^^^^^^^^^^^^^^

``{$and: [ query1, query2, ...] }``

`$and` performs a logical `AND` operation on an array of two or more subqueries.

.. note::

 | The `$or` and `$and` operators can be nested together,
 | **Example:** ``{z: 33, $and : [ {$or: [{a: 1}, {b: 2}]}, {$or: [{c: 5}, {d: 7}]} ] }``


.. _$gt:
.. _$gte:
.. _$lt:
.. _$lte:
.. _$bt:

Conditions on numeric values
****************************

Comparison operators `$gt`, `$gte` and `$lt`, `$lte`
are used for numeric datatypes.

* `$gt` Value greater than `>`
* `$gte` Value greater than or equal to `>=`
* `$lt` Value lesser than `<`
* `$lte` Value lesser than or equal to `<=`
* `$bt` Value within the specified range inclusively. ``{fieldpath : {$bt : [lower, upper]}}``

**Example:** find all persons with `age >= 38`:

.. code-block:: js

     ejdb> db.addressbook.find({age: {$gte: 38}});

**Example:** find all persons with `age >= 38 and age <= 40`:

.. code-block:: js

     ejdb> db.addressbook.find({age: {$bt: [38, 40]}});


.. _$stror:

$stror
******

String tokens matching:


``{fieldpath:  {$stror: [value1, value2, ....]}``

* If the `fieldpath` holds a `string` value, the `$stror` operator converts this value
  into an array of string tokens by splitting original value into a set of tokens separated by space `' '`
  or comma `','` characters. Then the operator selects documents whose set of tokens contains any token
  specified in `$stror` array ``[value1, value2, ...]``.

* If the `fieldpath` value is a string `array` the `$stror` operator selects
  documents whose `fieldpath` array contains any tokens specified in
  `$stror` array ``[value1, value2, ...]``.

.. _$stror_example:

**Example:**

.. code-block:: js

    ejdb> db.save('books', {'title' : 'All the Light We Cannot See'});
    ejdb> db.save('books', {'title' : 'Little Blue Truck Board Book'});
    ejdb> db.save('books', {'title' : 'The Book with No Pictures'});

    ejdb> db.books.find({title : {$icase : {$stror : ['book', 'light']}}});
    Found 3 records
    { _id: '55365fa019808d3c00000000',
      title: 'All the Light We Cannot See' }
    { _id: '55365fcb19808d3c00000001',
      title: 'Little Blue Truck Board Book' }
    { _id: '55365ff819808d3c00000002',
      title: 'The Book with No Pictures' }


.. _$strand:

$strand
*******

String tokens matching:


``{fieldpath:  {$strand: [value1, value2, ....]}``

* If the `fieldpath` holds a `string` value the `$strand` operator converts this value
  into an array of string tokens by splitting original value into a set of tokens separated by space `' '`
  or comma `','` characters. Then the operator selects documents whose set of tokens contains all
  tokens specified in `$strand` array ``[value1, value2, ...]``.

* If the `fieldpath` value is a string `array` the `$strand` operator selects
  documents whose `fieldpath` array contains all tokens specified in
  `$strand` array ``[value1, value2, ...]``.

  See :ref:`$stror example <$stror_example>`


.. _$exists:

$exists
*******

:term:`Fieldpath` existence checking:


``{fieldpath: {$exists: true|false}}``

When `$exists` value set to `true`, the documents that contain the `fieldpath` will be matched,
including documents where the value of `fieldpath` is null. Otherwise this operator returns the documents
that do not contain the specified `fieldpath`.

.. _$elemMatch:

$elemMatch
**********

Array element matching:


``{fieldpath: {$elemMatch: query}}}``

The `$elemMatch` operator matches `fieldpath` array values against the specified `query`

**Example:**

.. code-block:: js

    ejdb> db.save('persons', {name: 'Andy',
                              childs: [
                                        {name: 'Garry', age: 2},
                                        {name: 'Sally', age: 4}
                                      ]
                              });

    ejdb> db.persons.find({childs : {$elemMatch : {name: 'Garry', age:2}}});
    Found 1 records
    { _id: '5536764019808d3c00000004',
      name: 'Andy',
      childs:
       [ { name: 'Garry', age: 2 },
         { name: 'Sally', age: 4 } ] }

If you specify only a single query condition in the `$elemMatch` operator, you do not need to use `$elemMatch`:

.. code-block:: js

    ejdb> db.persons.find({'childs.name' : 'Garry'});
    // This is equivalent to:
    ejdb> db.persons.find({childs : {$elemMatch : {name: 'Garry'}}});

.. note::

    Only one `$elemMatch` operator is allowed in the context of one array `fieldpath`.


.. _qhints:

Query hints
-----------

.. _$max:

$max
****

The maximum number of documents retrieved.


.. _$skip:

$skip
*****

The number of skipped results in the result set


.. _$orderby:

$orderby
********

The sorting order of query fields specified as JSON mapping of document `fieldpaths`
to its orderby modes:

``{$orderby: {'fieldpath': mode, ...}``

Where `mode` is and integer specified sort order:

* `-1` Descending sort
* `1` Ascending sort

**Example:**

.. code-block:: js

   db.addressbook.find({}, {$orderby: {age:1, name:-1}});


.. _$fields:

$fields
*******

The document fields projection.

``{$fields: {'fieldpath': mode, ...}``

Where `mode` is an integer specified the field inclusion mode:

* `-1` Exclude field
* `1` Include field

.. note::

    `$fields` hint cannot mix include and exclude fields together

The mongodb `$ (projection) <http://docs.mongodb.org/manual/reference/operator/projection/positional/#proj._S_>`_ is also supported.
Our implementation overcomes the mongodb restriction:
`Only one array field can appear in the query document`

.. _$(projection):

$ (projection)
^^^^^^^^^^^^^^

``{$fields: {'prefix.$[.postfix]' : 1}``

The key `$` within the `$fields`_ projection limits the contents of an `array` field
returned as query results to contain only the first element matching the query. The `$` letter
means here the array index of the mached record.

**Example:**

.. code-block:: js

    // Not using $ projection
    ejdb> db.persons.find({
                            childs : {$elemMatch :
                                        {name: 'Garry', age:2}}
                          }, {$fields : {'childs' : 1}});
    Found 1 records
    { childs:
       [ { name: 'Garry', age: 2 },
         { name: 'Sally', age: 4 } ] }


    // Usign $ projection
    ejdb> db.persons.find({
                            childs : {$elemMatch :
                                        {name: 'Garry', age:2}}
                          }, {$fields : {'childs.$' : 1}});
    Found 1 records
    { _id: '5536764019808d3c00000004',
      childs: [ { name: 'Garry', age: 2 } ] }

`$` array projection can be in middle of `fieldpath`:

**Example:**

.. code-block:: js

    ejdb> db.save('records',
                  {z: 44,
                   arr: [ { h: 1 }, { h: 2, g: 4 } ]
                  });

    ejdb> db.records.find({z: 44, arr: {$elemMatch: {h: 2}} }, {$fields: {'arr.$.h': 1}});
    Found 1 records
    { _id: '55368bda19808d3c00000007',
      arr: [ { h: 2 } ] }


.. note::

    Our implementation overcomes the following mongodb projection limitation:
    `Only one array field can appear in the query document <http://docs.mongodb.org/manual/reference/operator/projection/positional/#array-field-limitations>`_
    You are allowed to use the `$` array projections for many fields simultaneously within one query.


Data manipulation
-----------------

.. _$set:

$set
****

``{$set: {fieldpath1: value1, ... } }``

The `$set` directive sets the value of the specified fields.

If the `fieldpath` does not exist in the document, `$set` will add a new fields with the specified value(s).
The `$set` can create all required subdocuments within the updated documents in order to ensure what `fieldpath`
exists in each of them. If you specify multiple field-value pairs, `$set` will update or create each field.

**Example:**

.. code-block:: js

    ejdb> db.save('coll', {});
    ejdb> db.coll.find();
    Found 1 records
    { _id: '553697b1d131946100000001' }

    ejdb> db.coll.update({'$set':{'foo.bar':'text'}});
    ejdb> db.coll.find()
    Found 1 records
    { _id: '5536934bd131946100000000',
      foo: { bar: 'text' } }


.. _$upsert:

$upsert
*******

``{query, $upsert : {fieldpath1: value1, fieldpath2: value2, ...}}``

Atomic upsert. If documents matched to the specified `query` are found, then `$upsert` will perform a `$set`_
operation, otherwise a new document will be inserted with its fields being initialised to
the provided values.

**Example:**

.. code-block:: js

    ejdb> db.books.find();
    Found 0 records

    //Insert
    ejdb> db.books.update({isbn:'0123456789',
                          '$upsert': {isbn:'0123456789', 'name':'my book'}});
    ejdb> db.books.find();
    Found 1 records
    { _id: '5536a054d131946100000002',
      isbn: '0123456789',
      name: 'my book' }

    //Update
    ejdb> db.books.update({isbn:'0123456789',
                          '$upsert': {isbn:'0123456789', 'name':'my old book'}});
    ejdb> db.books.find();
    Found 1 records
    { _id: '5536a054d131946100000002',
      isbn: '0123456789',
      name: 'my old book' }

.. _$dropall:

$dropall
********

``{query, $dropall : true}``

In-place document removal operation. All documents matched the specified `query`
will be removed from collection.

**Example:**

.. code-block:: js

    ejdb> db.books.find();
    Found 3 records
    { _id: '55365fa019808d3c00000000',
      title: 'All the Light We Cannot See' }
    { _id: '55365fcb19808d3c00000001',
      title: 'Little Blue Truck Board Book' }
    { _id: '55365ff819808d3c00000002',
      title: 'The Book with No Pictures' }

    //Remove all books with `title` contains a `Book` token.
    ejdb> db.books.update({title: {$strand: ['Book']}, $dropall:true});

    ejdb> db.books.find();
    Found 1 records
    { _id: '55365fa019808d3c00000000',
      title: 'All the Light We Cannot See' }


.. _$inc:

$inc
****

``{$inc: {fieldpath1: delta1, fieldpath2: delta2, ... }}``

Increment numeric field value by specified `delta`. The increment `delta`
can be of positive or negative number. The `$inc` operator does not create the specified
`fieldpath` if it is not exists in the document.

**Example:**

.. code-block:: js

    ejdb> db.save('inc', {counter:0});
    ejdb> db.update('inc', {$inc: {counter:-2}});
    ejdb> db.inc.find();
    Found 1 records
    { _id: '55373cb619808d3c00000009',
      counter: -2 }


.. _$unset:

$unset
******

``{$unset: {fieldpath1: "", fiedlpath2: "", ...}``

The `$unset` operator deletes the document fields specified by `fieldpath`.
The unset `fieldpath` values `""` used here in order to be comparable with
`mongodb $unset operation <http://docs.mongodb.org/manual/reference/operator/update/unset/>`_

`$unset` can be used together with `$ (update)`_ operator:

**Example:**

.. code-block:: js

    ejdb> db.coll.find()
    Found 1 records
    { _id: '5537447f19808d3c0000000a',
      a: [ 'b', 'cc', 'd' ] }

    //Then apply unset to the `a.cc` array element
    ejdb> db.coll.update({'a':'cc', $unset : {'a.$':''}});

    ejdb> db.coll.find();
    Found 1 records
    { _id: '5537447f19808d3c0000000a',
      a: [ 'b', undefined, 'd' ] }


.. _$ (update):

$ (update)
**********

The positional `$` operator identifies an element in an array
to update the position of the element in the array without explicitly specifying it.

**Example:**

.. code-block:: js

    ejdb> db.save('coll', {a : ['b','c','d']});
    ejdb> db.coll.find();
    Found 1 records
    { _id: '5537447f19808d3c0000000a',
      a: [ 'b', 'c', 'd' ] }

    //Then update with positional 'a.$'
    ejdb> db.coll.update({'a':'c', $set : {'a.$':'cc'}});

    ejdb> db.coll.find()
    Found 1 records
    { _id: '5537447f19808d3c0000000a',
      a: [ 'b', 'cc', 'd' ] }

If the specified array `fieldpath` is not contained in the query, the `$ (update)`
operator has no effect it that case.


**Example:**

.. code-block:: js

    ejdb> db.coll.find();
    Found 1 records
    { _id: '5537447f19808d3c0000000a',
      a: [ 'b', c, 'd' ],
      c: 11 }

    // Note: `a` field is not contained in the query:
    ejdb> db.coll.update({'c':11, $unset : {'a.$':''}});

    //Document remains unchanged
    ejdb> db.coll.find();
    Found 1 records
    { _id: '5537447f19808d3c0000000a',
      a: [ 'b', c, 'd' ],
      c: 11 }



.. _$addToSet:
.. _$addToSetAll:

$addToSet and $addToSetAll
**************************

``{query, $addToSet: {fieldpath1: value1, fieldpath2: value2, ...}}``

Add a specified value to the array only if it was not in the array.
This is atomic operation.

`$addToSetAll` is the batch version of `$addToSet` operator:

``{query, $addToSetAll: {fieldpath1: [...], fieldpath2: [...], ...}}``

Add a set of values to the array of every `fieldpath` specified in the query.
In this case every value will be added only if it was not contained in the target array.

.. seealso::
    The `$addToSet` and `$addToSetAll` are the dual operations to
    `$pull`_ and  `$pullAll`_

**Example:**

.. code-block:: js

    ejdb> db.songs.find();
    Found 1 records
    { _id: '553761dd19808d3c0000000b',
      name: 'Let It Be',
      tags: [] }

    //Add some tags:
    db.songs.update(
        {_id:'553761dd19808d3c0000000b',
            $addToSetAll: {
                tags:['the beatles', 'rock', '60s']
            }
    });

    ejdb> db.songs.find();
    Found 1 records
    { _id: '553761dd19808d3c0000000b',
      name: 'Let It Be',
      tags: [ 'the beatles', 'rock', '60s' ] }


    //One more tag:
    db.songs.update(
        {_id:'553761dd19808d3c0000000b',
            $addToSetAll: {
                tags:['the beatles', 'rock', '60s', 'classic rock']
            }
    });

    //All elements in `tags` being merged
    // with the passed tags array:
    ejdb> db.songs.find();
    Found 1 records
    { _id: '553761dd19808d3c0000000b',
      name: 'Let It Be',
      tags:
       [ 'the beatles',
         'rock',
         '60s',
         'classic rock' ] }



.. _$pull:
.. _$pullAll:

$pull and $pullAll
******************

``{query, $pull: {fieldpath1: value1, fieldpath2: value2, ...}}``

Remove a specified `value` from the array field pointed by `fieldpath`.
This is atomic operation.

`$pullAll` is the batch version of `$pull` operator:

``{query, $pullAll: {fieldpath1: [...], fieldpath2: [...], ...}}``

.. seealso::
    The `$pull` and `$pullAll` are the dual operations to
    `$addToSet`_ and  `$addToSetAll`_


.. _$push:
.. _$pushAll:

$push and $pushAll
******************
.. versionadded:: 1.2.8

``{query, $push: {fieldpath1: value1, fieldpath2: value2, ...}}``

Appends a specified `value` to the array field pointed by `fieldpath`.
If the `fieldpath` is missing in the document, the `$push` adds new array field containing the specified `value`.
This is atomic operation.

`$pushAll` is the batch version of `$push` operator:

``{query, $pushAll: {fieldpath1: [...], fieldpath2: [...], ...}}``


.. _$rename:

$rename
*******

``{query, $rename' : {fieldpath1 : name1, fieldpath2 : name2, ...}}``

Sets a new `name` to the field pointed by `fieldpath`.
If the document has already had a field with the specified `name`,
the `$rename` operator removes that field and renames the field pointed by `fieldpath`
to the new `name`.


.. _$slice (projection):

$slice (projection)
*******************

1. ``${..., $do: {fieldpath : {$slice : limit}}``
2. ``${..., $do: {fieldpath : {$slice : [offset, limit]}}``

The `$slice` operator is used in the context of `$do`_ directive and
limits a number of array items returned for document fields pointed by `fieldpath`.

Only non negative offsets are supported by the `$slice` projection. (EJDB |ejdbversion|)

**Example:**

.. code-block:: js

    ejdb> db.songs.find();
    Found 1 records
    { _id: '553761dd19808d3c0000000b',
      name: 'Let It Be',
      tags:
       [ 'the beatles',
         'rock',
         '60s',
         'classic rock' ] }

    //Apply a $slice limiting a `tags` array
    // to the first two elements
    ejdb> db.songs.find({$do : {tags : {$slice : 2}}});
    Found 1 records
    { _id: '553761dd19808d3c0000000b',
      name: 'Let It Be',
      tags: [ 'the beatles', 'rock' ] }


    //Lets skip a first two and load up-to ten items
    ejdb> db.songs.find({$do : {tags : {$slice : [2,10]}}});
    Found 1 records
    { _id: '553761dd19808d3c0000000b',
      name: 'Let It Be',
      tags: [ '60s', 'classic rock' ] }


.. _$do:

$do action directive
--------------------

The `$do` action directive is used in the following cases:

* :ref:`joins`
* Array `$slice (projection)`_ operator


Some performance considerations
-------------------------------

* Only one index can be used in search query operation.

* Negate operations: `$not`_ and `$nin`_ do not use indexes
  so they can be slow in comparison with other matching operations.

* It is better to execute update queries with `JBQRYCOUNT` control flag set
  to avoid unnecessarily data fetching. (C API)

Glossary
--------

.. glossary::

    OID
    ObjectId
        ObjectId (OID) is a unique identifier of every document stored in EJDB collection.
        You can consider OID as document's primary key. OID is a 12-byte value integer the same
        format as `defined in Mongodb BSON specification <http://docs.mongodb.org/manual/reference/object-id/>`_.

    abstract document
        Abstract document is a  metadocument for all the documents included in a particular collection without an actual
        field values and with all the possible document fields and their types belonging to any document of the collection.
        You can consider `abstract document` as a prototype for all the possible documents of the collection.

    fieldpath
        In the context of an :term:`abstract document` a `fieldpath`
        is the path consisting of JSON field names traversed from the document
        root to the particular document field joined together by `.` symbol.





