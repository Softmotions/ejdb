:orphan:


.. The EJDB documentation

Welcome to EJDB!
================

.. epigraph::

   No matter where you go, there you are.

   -- Buckaroo Banzai


EJDB aims to be a fast MongoDB-like library which can be embedded into C/C++,
.Net, NodeJS, Python, Lua, Java, Ruby, Go applications under terms of LGPL license.


.. contents::

This is a paragraph that contains `a link`_.

Tables
------

+------------------------+------------+----------+----------+
| Header row, column 1   | Header 2   | Header 3 | Header 4 |
| (header rows optional) |            |          |          |
+========================+============+==========+==========+
| body row 1, column 1   | column 2   | column 3 | column 4 |
+------------------------+------------+----------+----------+
| body row 2             | ...        | ...      |          |
+------------------------+------------+----------+----------+

Second table:

=====  =====  =======
A      B      A and B
=====  =====  =======
False  False  False
True   False  False
False  True   False
True   True   True
=====  =====  =======


The list table:

.. list-table:: Frozen Delights!
    :widths: 15 10 30
    :header-rows: 1

    *   - Treat
        - Quantity
        - Description
    *   - Albatross
        - 2.99
        - On a stick!
    *   - Crunchy Frog
        - 1.49
        - If we took the bones out, it wouldn't be
            crunchy, now would it?
    *   - Gannet Ripple
        - 1.99
        - On a stick!


Images
------

.. image:: _static/images/ejdblogo3.png
    :alt: alternate text
    :align: right


Misc
----

.. c:function:: PyObject* PyType_GenericAlloc(PyTypeObject *type, Py_ssize_t nitems)

    Simple `C` function.

.. envvar:: PATH

.. productionlist::
    try_stmt: try1_stmt | try2_stmt
    try1_stmt: "try" ":" `suite`
        : ("except" [`expression` ["," `target`]] ":" `suite`)+
        : ["else" ":" `suite`]
        : ["finally" ":" `suite`]
    try2_stmt: "try" ":" `suite`
        : "finally" ":" `suite`

.. hlist::
    :columns: 3

    * A list of
    * short items
    * that should be
    * displayed
    * horizontally

.. sidebar:: Sidebar Title
    :subtitle: Optional Sidebar Subtitle

    Subsequent indented lines comprise
    the body of the sidebar, and are
    interpreted as body elements.


E = mc\ :sup:`2`

H\ :sub:`2`\ O


| These lines are
| broken exactly like in
| the source file.


.. topic:: Topic Title

    Subsequent indented lines comprise
    the body of the topic, and are
    interpreted as body elements.


.. highlights::
    highlights

    * One
    * Two
    * Three


.. compound::

   The 'rm' command is very dangerous.  If you are logged
   in as root and enter ::

       cd /
       rm -rf *

   you will erase the entire contents of your file system.



Notes
-----

.. note::

   ``sphinx-apidoc`` generates reST files that use `sphinx.ext.autodoc` to
   document all found modules.  If any modules have side effects on import,
   these will be executed by ``autodoc`` when ``sphinx-build`` is run.

   If you document scripts (as opposed to library modules), make sure their main
   routine is protected by a ``if __name__ == '__main__'`` condition.



Some text


.. warning::

   ``sphinx-apidoc`` generates reST files that use `sphinx.ext.autodoc` to
   document all found modules.  If any modules have side effects on import,
   these will be executed by ``autodoc`` when ``sphinx-build`` is run.


.. error::

   ``sphinx-apidoc`` generates reST files that use `sphinx.ext.autodoc` to
   document all found modules.  If any modules have side effects on import,
   these will be executed by ``autodoc`` when ``sphinx-build`` is run.


Program options
---------------

The :program:`sphinx-quickstart` script has several options:

.. program:: sphinx-quickstart

.. option:: -q, --quiet

   Quiet mode that will skips interactive wizard to specify options.
   This option requires `-p`, `-a` and `-v` options.

.. option:: -h, --help, --version

   Display usage summary or Sphinx version.

.. seealso::
    `GNU tar manual, Basic Tar Format <http://link>`_
    Documentation for tar archive files, including GNU tar extensions.



**Function signature**

The :py:func:`enumerate` function can be used for ...

.. py:function:: enumerate(sequence[, start=0])

   Return an iterator that yields tuples of an index and an item of the
   *sequence*. (And so on.)


.. function:: repeat(object[, times])

   Make an iterator that returns *object* over and over again. Runs indefinitely
   unless the *times* argument is specified. Used as argument to :func:`map` for
   invariant parameters to the called function.  Also used with :func:`zip` to
   create an invariant part of a tuple record.  Equivalent to::

      def repeat(object, times=None):
          # repeat(10, 3) --> 10 10 10
          if times is None:
              while True:
                  yield object
          else:
              for i in range(times):
                  yield object

   A common use for *repeat* is to supply a stream of constant values to *map*
   or *zip*::

      >>> list(map(pow, range(10), repeat(2)))
      [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]




Styles
------
one asterisk: *text* for emphasis (italics),

two asterisks: **text** for strong emphasis (boldface), and

backquotes: ``text`` for code samples.

Subheading
**********

Hello


More topics to be covered
-------------------------

- Other extensions (math, viewcode, doctest)
- Static files
- Selecting a theme
- Templating
- Using extensions
- Subsection
- Writing extensions


Bullets 2
---------

* this is
* a list

  * with a nested list
  * and some subitems

* and here the parent list continues


Install Sphinx
--------------

Install Sphinx, either from a distribution package or from
`PyPI <https://pypi.python.org/pypi/Sphinx>`_ with ::

   $ pip install Sphinx


Code
----

The interpreter acts as a simple calculator: you can type an expression at it
and it will write the value.  Expression syntax is straightforward: the
operators ``+``, ``-``, ``*`` and ``/`` work just like in most other languages
(for example, Pascal or C); parentheses (``()``) can be used for grouping. See [#]_ and see [#f2]_

For example::

   >>> 2 + 2
   4
   >>> 50 - 5*6
   20
   >>> (50 - 5*6) / 4
   5.0
   >>> 8 / 5  # division always returns a floating point number
   1.6


Some random Python code:

.. code-block:: python
    :emphasize-lines: 3,5

    try:
        from DistUtilsExtra.command import *
    except ImportError:
        print >> sys.stderr, 'To build Sphinx-Theme-Brandenburg you need https://launchpad.net/python-distutils-extra'
        sys.exit(1)


    def read_from_file(path):
        with open(path) as input:
            return input.read()


Here we have some shell code:

.. code-block:: sh

    ## Go to a bookmark
    bcd(){
     if [ 2 -eq 0 ]; then
	 _bcd_help
     elif  _bcd_check_cfg_file; then
       dir=""
       if [ -z "" ]; then
	 echo "No bookmark \"borland\""
       else
	 cd ""
       fi
     fi
    }


Example

.. code-block:: python
    :linenos:

    >>> 2 + 2
    4
    >>> 50 - 5*6
    20
    >>> (50 - 5*6) / 4
    5.0
    >>> 8 / 5  # division always returns a floating point number
    1.6



Lorem ipsum [Ref]_ dolor sit amet.

.. _a link: http://example.com/



.. glossary::
    configuration directory
        The definition of term


.. rubric:: Footnotes

.. [#] This is the usual layout.  However, :file:`conf.py` can also live in
       another directory, the :term:`configuration directory`.
.. [#f1] Text of the first footnote.
.. [#f2] Text of the second footnote.
.. [Ref] Book or article reference, URL or whatever.

