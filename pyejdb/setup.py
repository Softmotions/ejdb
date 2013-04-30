#!/usr/bin/python3


try:
    from setuptools import setup, Feature
except ImportError:
    from distribute_setup import use_setuptools
    use_setuptools()
    from setuptools import setup, Feature

from platform import python_version
from distutils.command.build_ext import build_ext as _build_ext
from distutils.core import setup, Extension, Command

from ctypes.util import find_library
from ctypes import cdll, c_char_p

import sys
import os

py_ver = python_version()
min_py_vers = {3: "3.2.0"}
if int(py_ver[0]) < 3 or py_ver < min_py_vers[int(py_ver[0])]:
    raise SystemExit("Aborted: EJDB requires Python3 >= {0[3]}".format(min_py_vers))

if os.name != "posix":
    raise SystemExit("Aborted: os '{0}' not supported".format(os.name))


class TestCommand(Command):
    """Command for running unittests without install."""

    user_options = [("args=", None, '''The command args string passed to
                                    unittest framework, such as
                                     --args="-v -f"''')]

    def initialize_options(self):
        self.args = ''
        pass

    def finalize_options(self):
        pass

    def run(self):
        self.run_command('build')
        bld = self.distribution.get_command_obj('build')
        #Add build_lib in to sys.path so that unittest can found DLLs and libs
        sys.path = [os.path.abspath(bld.build_lib)] + sys.path

        import shlex
        import unittest

        test_argv0 = [sys.argv[0] + ' test --args=']
        #For transfering args to unittest, we have to split args
        #by ourself, so that command like:
        #python setup.py test --args="-v -f"
        #can be executed, and the parameter '-v -f' can be
        #transfering to unittest properly.
        test_argv = test_argv0 + shlex.split(self.args)
        unittest.main(module=None, defaultTest='test.test_suite', argv=test_argv)


class EJDBPythonExt(object):
    def __init__(self, required, libname, name, min_ver, ver_char_p, url,
                 *args, **kwargs):
        self.required = required
        self.libname = libname
        self.name = name
        self.min_ver = min_ver
        self.ver_char_p = ver_char_p
        self.url = url
        kwargs["libraries"].append("rt")
        self.c_ext = Extension(*args, **kwargs)

err_msg = """Aborted: pyejdb requires {0} >= {1} to be installed.
See {2} for more information."""

def check_extension(ext):
    lib = find_library(ext.libname)
    if ext.required and not lib:
        raise SystemExit(err_msg.format(ext.name, ext.min_ver, ext.url))
    elif lib:
        curr_ver = c_char_p.in_dll(cdll.LoadLibrary(lib), ext.ver_char_p).value
        if curr_ver.decode() < ext.min_ver:
            raise SystemExit(err_msg.format(ext.name, ext.min_ver, ext.url))
    return lib

ejdb_ext = EJDBPythonExt(True, "tcejdb", "EJDB", "1.1.3",
                         "tcversion", "http://ejdb.org",
                         "_pyejdb", ["src/pyejdb.c"],
                         libraries=["tcejdb", "z", "pthread", "m", "c"],
                         extra_compile_args=["-std=c99", "-Wall"])

class build_ext(_build_ext):
    def finalize_options(self):
        _build_ext.finalize_options(self)
        if "sdist" not in sys.argv:
            self.extensions = [ext.c_ext for ext in (ejdb_ext,) if check_extension(ext)]

setup(
    name="pyejdb",
    version="1.0.5",
    url="http://ejdb.org",
    keywords=["ejdb", "tokyocabinet", "nosql", "database", "storage", "embedded", "mongodb", "json"],
    description="Python3 binding for EJDB database engine.",
    long_description=open("README.md", "r").read(),
    author="Adamansky Anton",
    author_email="adamansky@gmail.com",
    platforms=["POSIX"],
    license="GNU Lesser General Public License (LGPL)",
    packages=["pyejdb"],
    cmdclass={
        "build_ext": build_ext,
        "test": TestCommand,
        },
    ext_modules=[ejdb_ext.c_ext],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Lesser General Public License v2 (LGPLv2)",
        "Operating System :: POSIX",
        "Programming Language :: Python :: 3.2",
        "Programming Language :: Python :: 3",
        "Topic :: Software Development :: Libraries"
    ],
    data_files = [("doc", ["README.md"])]
)