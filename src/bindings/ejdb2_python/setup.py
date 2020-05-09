#!/usr/bin/env python3
# -*- coding: utf8 -*-

from ctypes import c_char_p, cdll
from ctypes.util import find_library
from platform import python_version

from setuptools import Extension, Feature, setup
from distutils.version import StrictVersion
from distutils.command.build_ext import build_ext as _build_ext

import sys

py_ver = python_version()
min_py_vers = {3: "3.6"}

if py_ver < min_py_vers[int(py_ver[0])]:
    raise SystemExit("Aborted: EJDB2 requires Python >= {0}".format(
        min_py_vers[int(py_ver[0])]))


class EJDB2PythonExt(object):
    def __init__(self, required, libname, name, min_ver, ver_char_p, url, *args, **kwargs):
        self.required = required
        self.libname = libname
        self.name = name
        self.min_ver = min_ver
        self.ver_char_p = ver_char_p
        self.url = url
        self.c_ext = Extension(*args, **kwargs)


err_msg = """Aborted: pyejdb2 requires {0} >= {1} to be installed.
See {2} for more information."""


def check_extension(ext):
    lib = find_library(ext.libname)
    if ext.required and not lib:
        raise SystemExit(err_msg.format(ext.name, ext.min_ver, ext.url))
    elif lib:
        curr_ver = c_char_p.in_dll(cdll.LoadLibrary(lib), ext.ver_char_p).value
        if StrictVersion(curr_ver.decode() < StrictVersion(ext.min_ver)):
            raise SystemExit(err_msg.format(ext.name, ext.min_ver, ext.url))
    return lib


ejdb2_ext = EJDB2PythonExt(True, "ejdb2", "EJDB2", "2.0.48",
                           "ejdb2_version", "https://ejdb.org",
                           "_pyejdb2", ["src/pyejdb2.c"],
                           libraries=["ejdb2", "pthread", "m", "c"],
                           extra_compile_args=["-std=c11", "-Wall", "-Wno-declaration-after-statement"])


class BuildExt(_build_ext):
    def finalize_options(self):
        _build_ext.finalize_options(self)
        if "sdist" not in sys.argv:
            self.extensions = [ext._c_ext for ext in (ejdb2_ext,) if check_extension(ext)]


setup(
    name="pyejdb2",
    version="1.0.0",
    url="https://ejdb2.org",
    keywords=["ejdb", "ejdb2", "nosql", "json", "database", "storage", "embedded", "embeddable",
              "native", "binding"],
    description="Python binding for EJDB database engine.",
    long_description=open("README.md", "r").read(),
    author="Adamansky Anton",
    author_email="adamansky@gmail.com",
    license="MIT",
    packages=["pyejdb2"],
    cmdclass={
        "build_ext": BuildExt,
        # todo: "test": TestCommand
    },
    ext_modules=[ejdb2_ext.c_ext],
    data_files=[("doc", ["README.md"])],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: POSIX",
        "Operating System :: Unix",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: POSIX :: BSD :: FreeBSD",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Topic :: Database",
        "Topic :: Database :: Database Engines/Servers",
        "Topic :: Software Development :: Libraries"
    ]
)
