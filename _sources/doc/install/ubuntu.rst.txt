Ubuntu/Debian
=============

In order to install the latest `libejdb` distribution on
Ubuntu boxes you can use `EJDB Launchpad PPA <https://launchpad.net/~adamansky/+archive/ubuntu/ejdb>`_
repository.

.. code-block:: sh

    sudo add-apt-repository ppa:adamansky/ejdb
    sudo apt-get update
    sudo apt-get install ejdb ejdb-dbg

Building .deb packages
----------------------

Before building EJDB `.deb` packages you need the `debhelper` tool:

.. code-block:: sh

    sudo apt-get install debhelper

And satisfy basic prerequisites for `libejdb` source building:

* GNU make
* cmake >= 2.8.12
* gcc >= 4.7 or clang >= 3.4 C compiler
* zlib-dev

.. code-block:: sh

    mkdir ./build
    cd ./build
    cmake -DCMAKE_BUILD_TYPE=RelWithDebugInfo -DPACKAGE_DEB=ON

