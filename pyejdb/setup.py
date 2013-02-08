from platform import python_version
from os import name as os_name
from distutils.command.build_ext import build_ext as _build_ext
from distutils.core import setup, Extension
from ctypes.util import find_library
from ctypes import cdll, c_char_p
from sys import argv

py_ver = python_version()
min_py_vers = {2: "2.7.2", 3: "3.2.3"}
if py_ver < min_py_vers[int(py_ver[0])]:
    raise SystemExit("Aborted: ejdb requires Python2 >= {0[2]} "
                     "OR Python3 >= {0[3]}".format(min_py_vers))

if os_name != "posix":
    raise SystemExit("Aborted: os '{0}' not supported".format(os_name))

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

ejdb_ext = EJDBPythonExt(True, "tcejdb", "EJDB", "1.0.57",
                         "tcversion", "http://ejdb.org",
                         "pyejdb", ["pyejdb/pyejdb.c"],
                         libraries=["tcejdb", "z", "pthread", "m", "c"])

class build_ext(_build_ext):
    def finalize_options(self):
        _build_ext.finalize_options(self)
        if "sdist" not in argv:
            self.extensions = [ext.c_ext for ext in (ejdb_ext) if check_extension(ext)]

setup(
    name="pyejdb",
    version="1.0.1",
    url="http://ejdb.org",
    description="Python binding for EJDB database engine.",
    long_description=open("README.md", "r").read(),
    author="Adamansky Anton",
    author_email="adamansky@gmail.com",
    platforms=["POSIX"],
    license="GNU Lesser General Public License (LGPL)",
    packages=["pyejdb"],
    cmdclass={"build_ext": build_ext},
    ext_modules=[ejdb_ext.c_ext],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Lesser General Public License (LGPL)",
        "Operating System :: POSIX",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.2",
        "Topic :: Software Development :: Libraries"
    ]
)