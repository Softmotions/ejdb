from platform import python_version
from os import name as os_name


py_ver = python_version()
min_py_vers = {2 : "2.7.2", 3 : "3.2.3"}
if py_ver < min_py_vers[int(py_ver[0])]:
    raise SystemExit("Aborted: ejdb requires Python2 >= {0[2]} "
                     "OR Python3 >= {0[3]}".format(min_py_vers))


if os_name != "posix":
    raise SystemExit("Aborted: os '{0}' not supported".format(os_name))




