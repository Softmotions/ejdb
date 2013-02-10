#!/usr/bin/env python3
#-*- coding: utf8 -*-
################################################################################
#
# Parameter/return value type checking for Python 3000 using function annotations.
#
# (c) 2008-2012, Dmitry Dvoinikov <dmitry@targeted.org>
# Distributed under BSD license.
#
# Samples:
#
# from typecheck import *
#
# @typecheck
# def foo(i: int, x = None, s: str = "default") -> bool:
#     ...
#
# @typecheck
# def foo(*args, k1: int, k2: str = "default", k3 = None) -> nothing:
#     ...
#
# @typecheck
# def foo(ostream: with_attr("write", "flush"), f: optional(callable) = None):
#     ...
#
# divisible_by_three = lambda x: x % 3 == 0
# @typecheck
# def foo(i: by_regex("^[0-9]+$")) -> divisible_by_three:
#     ...
#
# @typecheck
# def reverse_2_tuple(t: (str, bytes)) -> (bytes, str):
#     ...
#
# @typecheck
# def reverse_3_list(t: [int, float, bool]) -> [bool, float, int]:
#     ...
#
# @typecheck
# def extract_from_dict(d: dict_of(int, str), k: tuple_of(int)) -> list_of(str):
#     ...
#
# @typecheck
# def set_level(level: one_of(1, 2, 3)):
#     ...
#
# @typecheck
# def accept_number(x: either(int, by_regex("^[0-9]+$"))):
#     ...
#
# @typecheck_with_exceptions(input_parameter_error = MemoryError):
# def custom_input_error(x: int): # now custom_input_error("foo") throws MemoryError
#     ...
#
# @typecheck_with_exceptions(return_value_error = TypeError):
# def custom_return_error() -> str: # now custom_return_error() throws TypeError
#     return 1
#
# The (6 times longer) source code with self-tests is available from:
# http://www.targeted.org/python/recipes/typecheck3000.py
#
################################################################################

__all__ = [

# decorators

"typecheck", "typecheck_with_exceptions",

# check predicates

"optional", "with_attr", "by_regex", "callable", "anything", "nothing",
"tuple_of", "list_of", "dict_of", "one_of", "either",

# exceptions

"TypeCheckError", "TypeCheckSpecificationError",
"InputParameterError", "ReturnValueError",

# utility methods

"disable",

]

################################################################################

import inspect
import functools
import re

callable = lambda x: hasattr(x, "__call__")
anything = lambda x: True
nothing = lambda x: x is None

################################################################################

_enabled = True

def disable():
    global _enabled
    _enabled = False

################################################################################

class TypeCheckError(Exception): pass
class TypeCheckSpecificationError(Exception): pass
class InputParameterError(TypeCheckError): pass
class ReturnValueError(TypeCheckError): pass

################################################################################

class Checker:

    class NoValue:
        def __str__(self):
            return "<no value>"
    no_value = NoValue()

    _registered = []

    @classmethod
    def register(cls, predicate, factory):
        cls._registered.append((predicate, factory))

    @classmethod
    def create(cls, value):
        if isinstance(value, cls):
            return value
        for predicate, factory in cls._registered:
            if predicate(value):
                return factory(value)
        else:
            return None

    def __call__(self, value):
        return self.check(value)

################################################################################

class TypeChecker(Checker):

    def __init__(self, cls):
        self._cls = cls

    def check(self, value):
        return isinstance(value, self._cls)

Checker.register(inspect.isclass, TypeChecker)

################################################################################

iterable = lambda x: hasattr(x, "__iter__")

class IterableChecker(Checker):

    def __init__(self, cont):
        self._cls = type(cont)
        self._checks = tuple(Checker.create(x) for x in iter(cont))

    def check(self, value):
        if not iterable(value):
            return False
        vals = tuple(iter(value))
        return isinstance(value, self._cls) and len(self._checks) == len(vals) and \
               functools.reduce(lambda r, c_v: r and c_v[0].check(c_v[1]),
                                zip(self._checks, vals), True)

Checker.register(iterable, IterableChecker)

################################################################################

class CallableChecker(Checker):

    def __init__(self, func):
        self._func = func

    def check(self, value):
        return bool(self._func(value))

Checker.register(callable, CallableChecker)

################################################################################

class OptionalChecker(Checker):

    def __init__(self, check):
        self._check = Checker.create(check)

    def check(self, value):
        return value is Checker.no_value or value is None or self._check.check(value)

optional = OptionalChecker

################################################################################

class WithAttrChecker(Checker):

    def __init__(self, *attrs):
        self._attrs = attrs

    def check(self, value):
        for attr in self._attrs:
            if not hasattr(value, attr):
                return False
        else:
            return True

with_attr = WithAttrChecker

################################################################################

class ByRegexChecker(Checker):

    _regex_eols = { str: "$", bytes: b"$" }
    _value_eols = { str: "\n", bytes: b"\n" }

    def __init__(self, regex):
        self._regex_t = type(regex)
        self._regex = re.compile(regex)
        self._regex_eol = regex[-1:] == self._regex_eols.get(self._regex_t)
        self._value_eol = self._value_eols[self._regex_t]

    def check(self, value):
        return type(value) is self._regex_t and \
               (not self._regex_eol or not value.endswith(self._value_eol)) and \
               self._regex.match(value) is not None

by_regex = ByRegexChecker

################################################################################

class TupleOfChecker(Checker):

    def __init__(self, check):
        self._check = Checker.create(check)

    def check(self, value):
        return isinstance(value, tuple) and \
               functools.reduce(lambda r, v: r and self._check.check(v), value, True)

tuple_of = TupleOfChecker

################################################################################

class ListOfChecker(Checker):

    def __init__(self, check):
        self._check = Checker.create(check)

    def check(self, value):
        return isinstance(value, list) and \
               functools.reduce(lambda r, v: r and self._check.check(v), value, True)

list_of = ListOfChecker

################################################################################

class DictOfChecker(Checker):

    def __init__(self, key_check, value_check):
        self._key_check = Checker.create(key_check)
        self._value_check = Checker.create(value_check)

    def check(self, value):
        return isinstance(value, dict) and \
               functools.reduce(lambda r, t: r and self._key_check.check(t[0]) and \
                                             self._value_check.check(t[1]),
                                value.items(), True)

dict_of = DictOfChecker

################################################################################

class OneOfChecker(Checker):

    def __init__(self, *values):
        self._values = values

    def check(self, value):
        return value in self._values

one_of = OneOfChecker

################################################################################

class EitherChecker(Checker):

    def __init__(self, *args):
        self._checks = tuple(Checker.create(arg) for arg in args)

    def check(self, value):
        for c in self._checks:
            if c.check(value):
                return True
        else:
            return False

either = EitherChecker

################################################################################

def typecheck(method, *, input_parameter_error = InputParameterError,
                         return_value_error = ReturnValueError):

    argspec = inspect.getfullargspec(method)
    if not argspec.annotations or not _enabled:
        return method

    default_arg_count = len(argspec.defaults or [])
    non_default_arg_count = len(argspec.args) - default_arg_count

    method_name = method.__name__
    arg_checkers = [None] * len(argspec.args)
    kwarg_checkers = {}
    return_checker = None
    kwarg_defaults = argspec.kwonlydefaults or {}

    for n, v in argspec.annotations.items():
        checker = Checker.create(v)
        if checker is None:
            raise TypeCheckSpecificationError("invalid typecheck for {0}".format(n))
        if n in argspec.kwonlyargs:
            if n in kwarg_defaults and \
               not checker.check(kwarg_defaults[n]):
                raise TypeCheckSpecificationError("the default value for {0} is incompatible "
                                                  "with its typecheck".format(n))
            kwarg_checkers[n] = checker
        elif n == "return":
            return_checker = checker
        else:
            i = argspec.args.index(n)
            if i >= non_default_arg_count and \
               not checker.check(argspec.defaults[i - non_default_arg_count]):
                raise TypeCheckSpecificationError("the default value for {0} is incompatible "
                                                  "with its typecheck".format(n))
            arg_checkers[i] = (n, checker)

    def typecheck_invocation_proxy(*args, **kwargs):

        for check, arg in zip(arg_checkers, args):
            if check is not None:
                arg_name, checker = check
                if not checker.check(arg):
                    raise input_parameter_error("{0}() has got an incompatible value "
                                                "for {1}: {2}".format(method_name, arg_name,
                                                                      str(arg) == "" and "''" or arg))

        for arg_name, checker in kwarg_checkers.items():
            kwarg = kwargs.get(arg_name, Checker.no_value)
            if not checker.check(kwarg):
                raise input_parameter_error("{0}() has got an incompatible value "
                                            "for {1}: {2}".format(method_name, arg_name,
                                                                  str(kwarg) == "" and "''" or kwarg))

        result = method(*args, **kwargs)

        if return_checker is not None and not return_checker.check(result):
            raise return_value_error("{0}() has returned an incompatible "
                                     "value: {1}".format(method_name, str(result) == "" and "''" or result))

        return result

    return functools.update_wrapper(typecheck_invocation_proxy, method,
                                    assigned = ("__name__", "__module__", "__doc__"))

################################################################################

_exception_class = lambda t: isinstance(t, type) and issubclass(t, Exception)

@typecheck
def typecheck_with_exceptions(*, input_parameter_error: optional(_exception_class) = InputParameterError,
                                 return_value_error: optional(_exception_class) = ReturnValueError):

    return lambda method: typecheck(method, input_parameter_error = input_parameter_error,
                                            return_value_error = return_value_error)

################################################################################

if __name__ == "__main__":

    print("self-testing module typecheck.py:")

    from expected import expected
    from time import time
    from random import randint, shuffle
    from traceback import extract_stack

    ############################################################################

    print("method proxy naming: ", end = "")

    ###################

    @typecheck
    def foo() -> nothing:
        pass

    assert foo.__name__ == "foo"

    ###################

    print("ok")

    ############################################################################

    print("avoiding excessive proxying: ", end = "")

    ###################

    @typecheck
    def foo():
        assert extract_stack()[-2][2] != "typecheck_invocation_proxy"

    foo()

    @typecheck
    def bar() -> nothing:
        assert extract_stack()[-2][2] == "typecheck_invocation_proxy"

    bar()

    ###################

    print("ok")

    ############################################################################

    print("double annotations wrapping: ", end = "")

    ###################

    @typecheck
    def foo(x: int):
        return x

    assert foo(1) == typecheck(foo)(1) == 1

    ###################

    print("ok")

    ############################################################################

    print("empty strings in incompatible values: ", end = "")

    ###################

    @typecheck
    def foo(s: lambda s: s != "" = None):
        return s

    assert foo() is None
    assert foo(None) is None
    assert foo(0) == 0

    with expected(InputParameterError("foo() has got an incompatible value for s: ''")):
        foo("")

    @typecheck
    def foo(*, k: optional(lambda s: s != "") = None):
        return k

    assert foo() is None
    assert foo(k = None) is None
    assert foo(k = 0) == 0

    with expected(InputParameterError("foo() has got an incompatible value for k: ''")):
        foo(k = "")

    @typecheck
    def foo(s = None) -> lambda s: s != "":
        return s

    assert foo() is None
    assert foo(None) is None
    assert foo(0) == 0

    with expected(ReturnValueError("foo() has returned an incompatible value: ''")):
        foo("")

    ###################

    print("ok")

    ############################################################################

    print("invalid typecheck: ", end = "")

    ###################

    with expected(TypeCheckSpecificationError("invalid typecheck for a")):
        @typecheck
        def foo(a: 10):
            pass

    with expected(TypeCheckSpecificationError("invalid typecheck for k")):
        @typecheck
        def foo(*, k: 10):
            pass

    with expected(TypeCheckSpecificationError("invalid typecheck for return")):
        @typecheck
        def foo() -> 10:
            pass

    ###################

    print("ok")

    ############################################################################

    print("incompatible default value: ", end = "")

    ###################

    with expected(TypeCheckSpecificationError("the default value for b is incompatible with its typecheck")):
        @typecheck
        def ax_b2(a, b: int = "two"):
            pass

    with expected(TypeCheckSpecificationError("the default value for a is incompatible with its typecheck")):
        @typecheck
        def a1_b2(a: int = "one", b = "two"):
            pass

    with expected(TypeCheckSpecificationError("the default value for a is incompatible with its typecheck")):
        @typecheck
        def foo(a: str = None):
            pass

    with expected(TypeCheckSpecificationError("the default value for a is incompatible with its typecheck")):
        @typecheck
        def kw(*, a: int = 1.0):
            pass

    with expected(TypeCheckSpecificationError("the default value for b is incompatible with its typecheck")):
        @typecheck
        def kw(*, a: int = 1, b: str = 10):
            pass

    ###################

    print("ok")

    ############################################################################

    print("changed default value: ", end = "")

    ###################

    @typecheck
    def foo(a: list = []):
        a.append(len(a))
        return a

    assert foo() == [0]
    assert foo() == [0, 1]
    assert foo([]) == [0]
    assert foo() == [0, 1, 2]
    assert foo() == [0, 1, 2, 3]

    ###################

    @typecheck
    def foo(*, k: optional(list) = []):
        k.append(len(k))
        return k

    assert foo() == [0]
    assert foo() == [0, 1]
    assert foo(k = []) == [0]
    assert foo() == [0, 1, 2]
    assert foo() == [0, 1, 2, 3]

    ###################

    print("ok")

    ############################################################################

    print("default vs. checked args: ", end = "")

    ###################

    @typecheck
    def axn_bxn(a, b):
        return a + b

    assert axn_bxn(10, 20) == 30
    assert axn_bxn(10, 20.0) == 30.0
    assert axn_bxn(10.0, 20) == 30.0
    assert axn_bxn(10.0, 20.0) == 30.0

    with expected(TypeError, "(?:axn_bxn\(\) takes exactly 2 (?:positional )?arguments \(1 given\)|"
                                "axn_bxn\(\) missing 1 required positional argument: 'b')"):
        axn_bxn(10)

    with expected(TypeError, "(?:axn_bxn\(\) takes exactly 2 (?:positional )?arguments \(0 given\)|"
                                "axn_bxn\(\) missing 2 required positional arguments: 'a' and 'b')"):
        axn_bxn()

    ###################

    @typecheck
    def axn_b2n(a, b = 2):
        return a + b

    assert axn_b2n(10, 20) == 30
    assert axn_b2n(10, 20.0) == 30.0
    assert axn_b2n(10.0, 20) == 30.0
    assert axn_b2n(10.0, 20.0) == 30.0

    assert axn_b2n(10) == 12
    assert axn_b2n(10.0) == 12.0

    with expected(TypeError, "(?:axn_b2n\(\) takes at least 1 (?:positional )?argument \(0 given\)|"
                                "axn_b2n\(\) missing 1 required positional argument: 'a')"):
        axn_b2n()

    ###################

    @typecheck
    def a1n_b2n(a = 1, b = 2):
        return a + b

    assert a1n_b2n(10, 20) == 30
    assert a1n_b2n(10, 20.0) == 30.0
    assert a1n_b2n(10.0, 20) == 30.0
    assert a1n_b2n(10.0, 20.0) == 30.0

    assert a1n_b2n(10) == 12
    assert a1n_b2n(10.0) == 12.0

    assert a1n_b2n() == 3

    ###################

    @typecheck
    def axn_bxc(a, b: int):
        return a + b

    assert axn_bxc(10, 20) == 30

    with expected(InputParameterError("axn_bxc() has got an incompatible value for b: 20.0")):
        axn_bxc(10, 20.0)

    assert axn_bxc(10.0, 20) == 30.0

    with expected(InputParameterError("axn_bxc() has got an incompatible value for b: 20.0")):
        axn_bxc(10.0, 20.0)

    with expected(TypeError, "(?:axn_bxc\(\) takes exactly 2 (?:positional )?arguments \(1 given\)|"
                                "axn_bxc\(\) missing 1 required positional argument: 'b')"):
        axn_bxc(10)

    with expected(TypeError, "(?:axn_bxc\(\) takes exactly 2 (?:positional )?arguments \(0 given\)|"
                                "axn_bxc\(\) missing 2 required positional arguments: 'a' and 'b')"):
        axn_bxc()

    ###################

    @typecheck
    def axn_b2c(a, b: int = 2):
        return a + b

    assert axn_b2c(10, 20) == 30

    with expected(InputParameterError("axn_b2c() has got an incompatible value for b: 20.0")):
        axn_b2c(10, 20.0)

    assert axn_b2c(10.0, 20) == 30.0

    with expected(InputParameterError("axn_b2c() has got an incompatible value for b: 20.0")):
        axn_b2c(10.0, 20.0)

    assert axn_b2c(10) == 12
    assert axn_b2c(10.0) == 12.0

    with expected(TypeError, "(?:axn_b2c\(\) takes at least 1 (?:positional )?argument \(0 given\)|"
                                "axn_b2c\(\) missing 1 required positional argument: 'a')"):
        axn_b2c()

    ###################

    @typecheck
    def a1n_b2c(a = 1, b: int = 2):
        return a + b

    assert a1n_b2c(10, 20) == 30

    with expected(InputParameterError("a1n_b2c() has got an incompatible value for b: 20.0")):
        a1n_b2c(10, 20.0)

    assert a1n_b2c(10.0, 20) == 30.0

    with expected(InputParameterError("a1n_b2c() has got an incompatible value for b: 20.0")):
        a1n_b2c(10.0, 20.0)

    assert a1n_b2c(10) == 12
    assert a1n_b2c(10.0) == 12.0

    assert a1n_b2c() == 3

    ###################

    @typecheck
    def axc_bxn(a: int, b):
        return a + b

    assert axc_bxn(10, 20) == 30
    assert axc_bxn(10, 20.0) == 30.0

    with expected(InputParameterError("axc_bxn() has got an incompatible value for a: 10.0")):
        axc_bxn(10.0, 20)

    with expected(InputParameterError("axc_bxn() has got an incompatible value for a: 10.0")):
        axc_bxn(10.0, 20.0)

    with expected(TypeError, "(?:axc_bxn\(\) takes exactly 2 (?:positional )?arguments \(1 given\)|"
                                "axc_bxn\(\) missing 1 required positional argument: 'b')"):
        axc_bxn(10)

    with expected(TypeError, "(?:axc_bxn\(\) takes exactly 2 (?:positional )?arguments \(0 given\)|"
                                "axc_bxn\(\) missing 2 required positional arguments: 'a' and 'b')"):
        axc_bxn()

    ###################

    @typecheck
    def axc_b2n(a: int, b = 2):
        return a + b

    assert axc_b2n(10, 20) == 30
    assert axc_b2n(10, 20.0) == 30.0

    with expected(InputParameterError("axc_b2n() has got an incompatible value for a: 10.0")):
        axc_b2n(10.0, 20)

    with expected(InputParameterError("axc_b2n() has got an incompatible value for a: 10.0")):
        axc_b2n(10.0, 20.0)

    assert axc_b2n(10) == 12

    with expected(InputParameterError("axc_b2n() has got an incompatible value for a: 10.0")):
        axc_b2n(10.0)

    with expected(TypeError, "(?:axc_b2n\(\) takes at least 1 (?:positional )?argument \(0 given\)|"
                                "axc_b2n\(\) missing 1 required positional argument: 'a')"):
        axc_b2n()

    ###################

    @typecheck
    def a1c_b2n(a: int = 1, b = 2):
        return a + b

    assert a1c_b2n(10, 20) == 30
    assert a1c_b2n(10, 20.0) == 30.0

    with expected(InputParameterError("a1c_b2n() has got an incompatible value for a: 10.0")):
        a1c_b2n(10.0, 20)

    with expected(InputParameterError("a1c_b2n() has got an incompatible value for a: 10.0")):
        a1c_b2n(10.0, 20.0)

    assert a1c_b2n(10) == 12

    with expected(InputParameterError("a1c_b2n() has got an incompatible value for a: 10.0")):
        a1c_b2n(10.0)

    assert a1c_b2n() == 3

    ###################

    @typecheck
    def axc_bxc(a: int, b: int):
        return a + b

    assert axc_bxc(10, 20) == 30

    with expected(InputParameterError("axc_bxc() has got an incompatible value for b: 20.0")):
        axc_bxc(10, 20.0)

    with expected(InputParameterError("axc_bxc() has got an incompatible value for a: 10.0")):
        axc_bxc(10.0, 20)

    with expected(InputParameterError("axc_bxc() has got an incompatible value for a: 10.0")):
        axc_bxc(10.0, 20.0)

    with expected(TypeError, "(?:axc_bxc\(\) takes exactly 2 (?:positional )?arguments \(1 given\)|"
                                "axc_bxc\(\) missing 1 required positional argument: 'b')"):
        axc_bxc(10)

    with expected(TypeError, "(?:axc_bxc\(\) takes exactly 2 (?:positional )?arguments \(0 given\)|"
                                "axc_bxc\(\) missing 2 required positional arguments: 'a' and 'b')"):
        axc_bxc()

    ###################

    @typecheck
    def axc_b2c(a: int, b: int = 2):
        return a + b

    assert axc_b2c(10, 20) == 30

    with expected(InputParameterError("axc_b2c() has got an incompatible value for b: 20.0")):
        axc_b2c(10, 20.0)

    with expected(InputParameterError("axc_b2c() has got an incompatible value for a: 10.0")):
        axc_b2c(10.0, 20)

    with expected(InputParameterError("axc_b2c() has got an incompatible value for a: 10.0")):
        axc_b2c(10.0, 20.0)

    assert axc_b2c(10) == 12

    with expected(InputParameterError("axc_b2c() has got an incompatible value for a: 10.0")):
        axc_b2c(10.0)

    with expected(TypeError, "(?:axc_b2c\(\) takes at least 1 (?:positional )?argument \(0 given\)|"
                                "axc_b2c\(\) missing 1 required positional argument: 'a')"):
        axc_b2c()

    ###################

    @typecheck
    def a1c_b2c(a: int = 1, b: int = 2):
        return a + b

    assert a1c_b2c(10, 20) == 30

    with expected(InputParameterError("a1c_b2c() has got an incompatible value for b: 20.0")):
        a1c_b2c(10, 20.0)

    with expected(InputParameterError("a1c_b2c() has got an incompatible value for a: 10.0")):
        a1c_b2c(10.0, 20)

    with expected(InputParameterError("a1c_b2c() has got an incompatible value for a: 10.0")):
        a1c_b2c(10.0, 20.0)

    assert a1c_b2c(10) == 12

    with expected(InputParameterError("a1c_b2c() has got an incompatible value for a: 10.0")):
        a1c_b2c(10.0)

    assert a1c_b2c() == 3

    ###################

    print("ok")

    ############################################################################

    print("default vs. checked args (randomly generated): ", end = "")

    ###################

    test_passes = 0

    start = time()
    while time() < start + 5.0:

        N = randint(1, 10)
        DN = randint(0, N)

        args = [ "a{0:03d}".format(i) for i in range(N) ]
        chkd = [ randint(0, 1) for i in range(N) ]
        deft = [ i >= DN for i in range(N) ]

        def_args = ", ".join(map(lambda x: "{0}{1}{2}".format(x[1][0], x[1][1] and ": int" or "",
                                                              x[1][2] and " = {0}".format(x[0]) or ""),
                                 enumerate(zip(args, chkd, deft))))

        sum_args = " + ".join(args)

        test = "@typecheck\n" \
               "def test_func({def_args}):\n" \
               "    return {sum_args}\n"

        for provided_args in range(DN, N + 1):

            success_args = [j * 10 for j in range(provided_args)]
            success_result = provided_args * (provided_args - 1) * 9 // 2 + N * (N - 1) // 2

            test_run = test
            test_run += "assert test_func({success_args}) == {success_result}\n"

            failure_args = [ j for (j, c) in enumerate(chkd) if j < provided_args and c ]
            if failure_args:
                shuffle(failure_args)
                failure_arg = failure_args[0]
                failure_value = failure_arg * 10.0
                failure_args = success_args[:]
                failure_args[failure_arg] = failure_value
                test_run += "with expected(InputParameterError('test_func() has got an " \
                                           "incompatible value for a{failure_arg:03d}: {failure_value}')):\n" \
                            "    test_func({failure_args})\n"
                failure_args = ", ".join(map(str, failure_args))

            success_args = ", ".join(map(str, success_args))
            exec(test_run.format(**locals()))

            test_passes += 1

    ###################

    print("{0} test passes ok".format(test_passes))

    ############################################################################

    print("default vs. checked kwargs: ", end = "")

    ###################

    @typecheck
    def foo(*, a: str):
        return a

    with expected(InputParameterError("foo() has got an incompatible value for a: <no value>")):
        foo()

    assert foo(a = "b") == "b"

    ###################

    @typecheck
    def foo(*, a: optional(str) = "a"):
        return a

    assert foo() == "a"
    assert foo(a = "b") == "b"

    with expected(InputParameterError("foo() has got an incompatible value for a: 10")):
        foo(a = 10)

    ###################

    @typecheck
    def pxn_qxn(*, p, q, **kwargs):
        return p + q

    assert pxn_qxn(p = 1, q = 2) == 3
    assert pxn_qxn(p = 1, q = 2.0) == 3.0
    assert pxn_qxn(p = 1.0, q = 2) == 3.0
    assert pxn_qxn(p = 1.0, q = 2.0) == 3.0

    with expected(TypeError, "(?:pxn_qxn\(\) needs keyword-only argument q|"
                                "pxn_qxn\(\) missing 1 required keyword-only argument: 'q')"):
        pxn_qxn(p = 1)

    with expected(TypeError, "(?:pxn_qxn\(\) needs keyword-only argument p|"
                                "pxn_qxn\(\) missing 1 required keyword-only argument: 'p')"):
        pxn_qxn(q = 2)

    with expected(TypeError, "(?:pxn_qxn\(\) needs keyword-only argument p|"
                                "pxn_qxn\(\) missing 2 required keyword-only arguments: 'p' and 'q')"):
        pxn_qxn()

    ###################

    @typecheck
    def pxn_q2n(*, p, q = 2):
        return p + q

    assert pxn_q2n(p = 1, q = 2) == 3
    assert pxn_q2n(p = 1, q = 2.0) == 3.0
    assert pxn_q2n(p = 1.0, q = 2) == 3.0
    assert pxn_q2n(p = 1.0, q = 2.0) == 3.0

    assert pxn_q2n(p = 1) == 3

    with expected(TypeError, "(?:pxn_q2n\(\) needs keyword-only argument p|"
                                "pxn_q2n\(\) missing 1 required keyword-only argument: 'p')"):
        pxn_q2n(q = 2)

    with expected(TypeError, "(?:pxn_q2n\(\) needs keyword-only argument p|"
                                "pxn_q2n\(\) missing 1 required keyword-only argument: 'p')"):
        pxn_q2n()

    ###################

    @typecheck
    def p1n_q2n(*, p = 1, q = 2):
        return p + q

    assert p1n_q2n(p = 1, q = 2) == 3
    assert p1n_q2n(p = 1, q = 2.0) == 3.0
    assert p1n_q2n(p = 1.0, q = 2) == 3.0
    assert p1n_q2n(p = 1.0, q = 2.0) == 3.0

    assert p1n_q2n(p = 1) == 3
    assert p1n_q2n(q = 2) == 3

    assert p1n_q2n() == 3

    ###################

    @typecheck
    def pxn_qxc(*, p, q: int):
        return p + q

    assert pxn_qxc(p = 1, q = 2) == 3

    with expected(InputParameterError("pxn_qxc() has got an incompatible value for q: 2.0")):
        pxn_qxc(p = 1, q = 2.0)

    assert pxn_qxc(p = 1.0, q = 2) == 3.0

    with expected(InputParameterError("pxn_qxc() has got an incompatible value for q: 2.0")):
        pxn_qxc(p = 1.0, q = 2.0)

    with expected(InputParameterError("pxn_qxc() has got an incompatible value for q: <no value>")):
        pxn_qxc(p = 1)

    with expected(TypeError, "(?:pxn_qxc\(\) needs keyword-only argument p|"
                                "pxn_qxc\(\) missing 1 required keyword-only argument: 'p')"):
        pxn_qxc(q = 2)

    with expected(InputParameterError("pxn_qxc() has got an incompatible value for q: <no value>")):
        pxn_qxc()

    ###################

    @typecheck
    def pxn_q2c(*, p, q: int = 2):
        return p + q

    assert pxn_q2c(p = 1, q = 2) == 3

    with expected(InputParameterError("pxn_q2c() has got an incompatible value for q: 2.0")):
        pxn_q2c(p = 1, q = 2.0)

    assert pxn_q2c(p = 1.0, q = 2) == 3.0

    with expected(InputParameterError("pxn_q2c() has got an incompatible value for q: 2.0")):
        pxn_q2c(p = 1.0, q = 2.0)

    with expected(InputParameterError("pxn_q2c() has got an incompatible value for q: <no value>")):
        pxn_q2c(p = 1)

    with expected(TypeError, "(?:pxn_q2c\(\) needs keyword-only argument p|"
                                "pxn_q2c\(\) missing 1 required keyword-only argument: 'p')"):
        pxn_q2c(q = 2)

    with expected(InputParameterError("pxn_q2c() has got an incompatible value for q: <no value>")):
        pxn_q2c()

    ###################

    @typecheck
    def p1n_q2c(*, p = 1, q: int = 2):
        return p + q

    assert p1n_q2c(p = 1, q = 2) == 3

    with expected(InputParameterError("p1n_q2c() has got an incompatible value for q: 2.0")):
        p1n_q2c(p = 1, q = 2.0)

    assert p1n_q2c(p = 1.0, q = 2) == 3.0

    with expected(InputParameterError("p1n_q2c() has got an incompatible value for q: 2.0")):
        p1n_q2c(p = 1.0, q = 2.0)

    with expected(InputParameterError("p1n_q2c() has got an incompatible value for q: <no value>")):
        p1n_q2c(p = 1)

    assert p1n_q2c(q = 2) == 3

    with expected(InputParameterError("p1n_q2c() has got an incompatible value for q: <no value>")):
        p1n_q2c()

    ###################

    @typecheck
    def pxc_qxc(*, p: int, q: int):
        return p + q

    assert pxc_qxc(p = 1, q = 2) == 3

    with expected(InputParameterError("pxc_qxc() has got an incompatible value for q: 2.0")):
        pxc_qxc(p = 1, q = 2.0)

    with expected(InputParameterError("pxc_qxc() has got an incompatible value for p: 1.0")):
        pxc_qxc(p = 1.0, q = 2)

    with expected(InputParameterError("pxc_qxc() has got an incompatible value for q: 2.0")):
        pxc_qxc(p = 1.0, q = 2.0)

    with expected(InputParameterError("pxc_qxc() has got an incompatible value for q: <no value>")):
        pxc_qxc(p = 1)

    with expected(InputParameterError("pxc_qxc() has got an incompatible value for p: <no value>")):
        pxc_qxc(q = 2)

    with expected(InputParameterError("pxc_qxc() has got an incompatible value for q: <no value>")):
        pxc_qxc()

    ###################

    @typecheck
    def pxc_q2c(*, p: int, q: int = 2):
        return p + q

    assert pxc_q2c(p = 1, q = 2) == 3

    with expected(InputParameterError("pxc_q2c() has got an incompatible value for q: 2.0")):
        pxc_q2c(p = 1, q = 2.0)

    with expected(InputParameterError("pxc_q2c() has got an incompatible value for p: 1.0")):
        pxc_q2c(p = 1.0, q = 2)

    with expected(InputParameterError("pxc_q2c() has got an incompatible value for q: 2.0")):
        pxc_q2c(p = 1.0, q = 2.0)

    with expected(InputParameterError("pxc_q2c() has got an incompatible value for q: <no value>")):
        pxc_q2c(p = 1)

    with expected(InputParameterError("pxc_q2c() has got an incompatible value for p: <no value>")):
        pxc_q2c(q = 2)

    with expected(InputParameterError("pxc_q2c() has got an incompatible value for q: <no value>")):
        pxc_q2c()

    ###################

    @typecheck
    def p1c_q2c(*, p: int = 1, q: int = 2):
        return p + q

    assert p1c_q2c(p = 1, q = 2) == 3

    with expected(InputParameterError("p1c_q2c() has got an incompatible value for q: 2.0")):
        p1c_q2c(p = 1, q = 2.0)

    with expected(InputParameterError("p1c_q2c() has got an incompatible value for p: 1.0")):
        p1c_q2c(p = 1.0, q = 2)

    with expected(InputParameterError("p1c_q2c() has got an incompatible value for q: 2.0")):
        p1c_q2c(p = 1.0, q = 2.0)

    with expected(InputParameterError("p1c_q2c() has got an incompatible value for q: <no value>")):
        p1c_q2c(p = 1)

    with expected(InputParameterError("p1c_q2c() has got an incompatible value for p: <no value>")):
        p1c_q2c(q = 2)

    with expected(InputParameterError("p1c_q2c() has got an incompatible value for q: <no value>")):
        p1c_q2c()

    ###################

    print("ok")

    ############################################################################

    print("default vs. checked kwargs (randomly generated): ", end = "")

    ###################

    test_passes = 0

    start = time()
    while time() < start + 5.0:

        N = randint(1, 10)

        kwrs = [ "k{0:03d}".format(i) for i in range(N) ]
        chkd = [ randint(0, 1) for i in range(N) ]
        deft = [ randint(0, 1) for i in range(N) ]

        def_kwrs = ", ".join("{0}{1}{2}".format(k, c and ": optional(int)" or "", d and " = {0}".format(i) or "")
                             for (i, (k, c, d)) in enumerate(zip(kwrs, chkd, deft)))
        sum_kwrs = " + ".join(kwrs)

        test = "@typecheck\n" \
               "def test_func(*, {def_kwrs}):\n" \
               "    return {sum_kwrs}\n"

        for i in range(N):

            success_kwrs = { k: i * 10 for (i, (k, c, d)) in enumerate(zip(kwrs, chkd, deft))
                                       if (not d) or randint(0, 1) }

            temp_kwrs = success_kwrs.copy()
            temp_kwrs.update({ k: i for (i, (k, d)) in enumerate(zip(kwrs, deft))
                                    if d and k not in success_kwrs })

            success_result = sum(temp_kwrs.values())

            test_run = test
            test_run += "kwargs = {success_kwrs}\n" \
                        "assert test_func(**kwargs) == {success_result}\n"

            failure_kwrs = success_kwrs.copy()
            for k, v in failure_kwrs.items():
                if chkd[int(k[1:])] and randint(0, 1):
                    failure_kwarg = k
                    failure_value = float(v)
                    failure_kwrs[failure_kwarg] = failure_value
                    test_run += "kwargs = {failure_kwrs}\n" \
                                "with expected(InputParameterError('test_func() has got an " \
                                               "incompatible value for {failure_kwarg}: {failure_value}')):\n" \
                                "    test_func(**kwargs)\n"
                    break

            exec(test_run.format(**locals()))

            test_passes += 1

    ###################

    print("{0} test passes ok".format(test_passes))

    ############################################################################

    print("TypeChecker: ", end = "")

    ###################

    @typecheck
    def foo(a: int) -> object:
        return a

    assert foo(10) == 10

    @typecheck
    def foo(*args, a: str) -> float:
        return float(a)

    assert foo(a = "10.0") == 10.0

    class Foo():
        pass

    class Bar(Foo):
        pass

    @typecheck
    def foo(a: Bar) -> Foo:
        return a

    f = Bar()
    assert foo(f) is f

    f = Foo()
    with expected(InputParameterError("foo() has got an incompatible value for a: <__main__.Foo object at")):
        foo(f)

    @typecheck
    def foo(a: Foo) -> Bar:
        return a

    f = Bar()
    assert foo(f) is f

    f = Foo()
    with expected(ReturnValueError("foo() has returned an incompatible value: <__main__.Foo object at")):
        foo(f)

    ###################

    print("ok")

    ############################################################################

    print("IterableChecker: ", end = "")

    ###################

    @typecheck
    def foo(a: () = (), *, k: optional(()) = ()) -> ((), ()):
        return a, k

    assert foo() == ((), ())
    assert foo(()) == ((), ())
    assert foo(k = ()) == ((), ())
    assert foo((), k = ()) == ((), ())

    with expected(InputParameterError("foo() has got an incompatible value for a: []")):
        foo([])

    with expected(InputParameterError("foo() has got an incompatible value for a: (1,)")):
        foo((1, ))

    with expected(InputParameterError("foo() has got an incompatible value for k: []")):
        foo(k = [])

    with expected(InputParameterError("foo() has got an incompatible value for k: (1,)")):
        foo(k = (1, ))

    ###################

    @typecheck
    def foo(a: [] = [], *, k: optional([]) = None) -> ([], optional([])):
        return a, k

    assert foo() == ([], None)
    assert foo([]) == ([], None)
    assert foo(k = []) == ([], [])
    assert foo([], k = []) == ([], [])

    with expected(InputParameterError("foo() has got an incompatible value for a: ()")):
        foo(())

    with expected(InputParameterError("foo() has got an incompatible value for a: (1,)")):
        foo((1, ))

    with expected(InputParameterError("foo() has got an incompatible value for k: ()")):
        foo(k = ())

    with expected(InputParameterError("foo() has got an incompatible value for k: (1,)")):
        foo(k = (1, ))

    ###################

    @typecheck
    def foo(*args) -> (int, str):
        return args

    foo(1, "2") == 1, "2"

    with expected(ReturnValueError("foo() has returned an incompatible value: (1, 2)")):
        foo(1, 2)

    with expected(ReturnValueError("foo() has returned an incompatible value: (1, '2', None)")):
        foo(1, "2", None)

    with expected(ReturnValueError("foo() has returned an incompatible value: (1,)")):
        foo(1)

    ###################

    @typecheck
    def foo(*, k: optional([[[[lambda x: x % 3 == 1]]]]) = [[[[4]]]]):
        return k[0][0][0][0]

    assert foo() % 3 == 1
    assert foo(k = [[[[1]]]]) % 3 == 1

    with expected(InputParameterError("foo() has got an incompatible value for k: [[[[5]]]]")):
        foo(k = [[[[5]]]])

    ###################

    print("ok")

    ############################################################################

    print("CallableChecker: ", end = "")

    ###################

    @typecheck
    def foo(a: callable, *, k: callable) -> callable:
        return a(k(lambda: 2))

    x = lambda x: x
    assert foo(x, k = x)() == 2

    ###################

    class NumberToolset:
        @classmethod
        @typecheck
        def is_even(cls, value: int) -> bool:
            return value % 2 == 0
        @staticmethod
        @typecheck
        def is_odd(value: int) -> bool:
            return not NumberToolset.is_even(value)

    @typecheck
    def foo(a: NumberToolset.is_even = 0) -> NumberToolset.is_odd:
        return a + 1

    assert foo() == 1
    assert foo(2) == 3

    with expected(InputParameterError("is_even() has got an incompatible value for value: 1.0")):
        foo(1.0)

    ###################

    @typecheck
    def foo(x = None) -> nothing:
        return x

    assert foo() is None

    with expected(ReturnValueError("foo() has returned an incompatible value: ''")):
        foo("")

    ###################

    print("ok")

    ############################################################################

    print("OptionalChecker: ", end = "")

    ###################

    @typecheck
    def foo(b: bool) -> bool:
        return not b

    assert foo(True) is False
    assert foo(False) is True

    with expected(InputParameterError("foo() has got an incompatible value for b: 0")):
        foo(0)

    @typecheck
    def foo(*, b: optional(bool) = None) -> bool:
        return b

    assert foo(b = False) is False

    with expected(ReturnValueError("foo() has returned an incompatible value: None")):
        foo()

    ###################

    not_none = lambda x: x is not None

    with expected(TypeCheckSpecificationError("the default value for a is incompatible with its typecheck")):
        @typecheck
        def foo(a: not_none = None):
            return a

    @typecheck
    def foo(a: optional(not_none) = None): # note how optional overrides the not_none
        return a

    assert foo() is None
    assert foo(None) is None

    with expected(TypeCheckSpecificationError("the default value for k is incompatible with its typecheck")):
        @typecheck
        def foo(*, k: not_none = None):
            return k

    @typecheck
    def foo(*, k: optional(not_none) = None): # note how optional overrides the not_none
        return k

    assert foo() is None
    assert foo(k = None) is None

    @typecheck
    def foo(x = None) -> not_none:
        return x

    with expected(ReturnValueError("foo() has returned an incompatible value: None")):
        foo()

    @typecheck
    def foo(x = None) -> optional(not_none): # note how optional overrides the not_none
        return x

    assert foo() is None
    assert foo(None) is None

    ###################

    print("ok")

    ############################################################################

    print("with_attr: ", end = "")

    ###################

    class FakeIO:
        def write(self):
            pass
        def flush(self):
            pass

    @typecheck
    def foo(a: with_attr("write", "flush")):
        pass

    foo(FakeIO())

    del FakeIO.flush

    with expected(InputParameterError("foo() has got an incompatible value for a: <__main__.FakeIO object at ")):
        foo(FakeIO())

    ###################

    assert with_attr("__class__")(int) and with_attr("__class__").check(int)
    assert not with_attr("foo")(int) and not with_attr("foo").check(int)

    ###################

    print("ok")

    ############################################################################

    print("by_regex: ", end = "")

    ###################

    assert by_regex("^abc$")("abc")
    assert not by_regex("^abc$")(b"abc")
    assert not by_regex(b"^abc$")("abc")
    assert by_regex(b"^abc$")(b"abc")

    assert by_regex(b"^foo\x00bar$")(b"foo\x00bar")
    assert not by_regex(b"^foo\x00bar$")(b"foo\x00baz")

    ###################

    assert by_regex("^abc")("abc\n")
    assert by_regex(b"^abc")(b"abc\n")

    assert not by_regex("^abc$")("abc\n")
    assert not by_regex(b"^abc$")(b"abc\n")

    assert not by_regex("^abc$")("abcx")
    assert not by_regex(b"^abc$")(b"abcx")

    ###################

    @typecheck
    def foo(*, k: by_regex("^[0-9A-F]+$")) -> by_regex("^[0-9]+$"):
        return "".join(reversed(k))

    assert foo(k = "1234") == "4321"

    with expected(InputParameterError("foo() has got an incompatible value for k: ''")):
        foo(k = "")

    with expected(InputParameterError("foo() has got an incompatible value for k: 1")):
        foo(k = 1)

    with expected(ReturnValueError("foo() has returned an incompatible value: DAB")):
        foo(k = "BAD")

    ###################

    @typecheck
    def foo(*, k: (by_regex("^1$"), [by_regex("^x$"), by_regex("^y$")])):
        return k[0] + k[1][0] + k[1][1]

    assert foo(k = ("1", ["x", "y"])) == "1xy"

    with expected(InputParameterError("foo() has got an incompatible value for k: ('2', ['x', 'y'])")):
        foo(k = ("2", ["x", "y"]))

    with expected(InputParameterError("foo() has got an incompatible value for k: ('1', ['X', 'y'])")):
        foo(k = ("1", ["X", "y"]))

    with expected(InputParameterError("foo() has got an incompatible value for k: ('1', ['x', 'Y'])")):
        foo(k = ("1", ["x", "Y"]))

    ###################

    russian = "\u0410\u0411\u0412\u0413\u0414\u0415\u0401\u0416\u0417\u0418\u0419\u041a" \
              "\u041b\u041c\u041d\u041e\u041f\u0420\u0421\u0422\u0423\u0424\u0425\u0426" \
              "\u0427\u0428\u0429\u042c\u042b\u042a\u042d\u042e\u042f\u0430\u0431\u0432" \
              "\u0433\u0434\u0435\u0451\u0436\u0437\u0438\u0439\u043a\u043b\u043c\u043d" \
              "\u043e\u043f\u0440\u0441\u0442\u0443\u0444\u0445\u0446\u0447\u0448\u0449" \
              "\u044c\u044b\u044a\u044d\u044e\u044f"

    assert len(russian) == 66

    @typecheck
    def foo(s: by_regex("^[{0}]$".format(russian))):
        return len(s)

    for c in russian:
        assert foo(c) == 1

    with expected(InputParameterError("foo() has got an incompatible value for s: @")):
        foo("@")

    ###################

    @typecheck
    def numbers_only_please(s: by_regex("^[0-9]+$")):
        pass

    numbers_only_please("123")

    with expected(InputParameterError("numbers_only_please() has got an incompatible value for s: 123")):
        numbers_only_please("123\x00HUH?")

    ###################

    assert by_regex("^123$")("123") and by_regex("^123$").check("123")
    assert not by_regex("^123$")("foo") and not by_regex("^123$").check("foo")

    ###################

    print("ok")

    ############################################################################

    print("tuple_of: ", end = "")

    ###################

    @typecheck
    def foo(x: tuple_of(int)) -> tuple_of(float):
        return tuple(map(float, x))

    assert foo(()) == ()
    assert foo((1, 2, 3)) == (1.0, 2.0, 3.0)

    with expected(InputParameterError("foo() has got an incompatible value for x: ('1.0',)")):
        foo(("1.0",))

    with expected(InputParameterError("foo() has got an incompatible value for x: []")):
        foo([])

    ###################

    @typecheck
    def foo(x: tuple_of([by_regex("^[01]+$"), int])) -> bool:
        return functools.reduce(lambda r, e: r and int(e[0], 2) == e[1],
                                x, True)

    assert foo((["1010", 10], ["0101", 5]))
    assert not foo((["1010", 10], ["0111", 77]))

    ###################

    assert tuple_of(optional(by_regex("^foo$")))(("foo", None, "foo")) and \
           tuple_of(optional(by_regex("^foo$"))).check(("foo", None, "foo"))

    assert not tuple_of(optional(by_regex("^foo$")))(("123", None, "foo")) and \
           not tuple_of(optional(by_regex("^foo$"))).check(("123", None, "foo"))

    ###################

    print("ok")

    ############################################################################

    print("list_of: ", end = "")

    ###################

    @typecheck
    def foo(x: list_of(int)) -> list_of(float):
        return list(map(float, x))

    assert foo([]) == []
    assert foo([1, 2, 3]) == [1.0, 2.0, 3.0]

    with expected(InputParameterError("foo() has got an incompatible value for x: ['1.0']")):
        foo(["1.0"])

    with expected(InputParameterError("foo() has got an incompatible value for x: ()")):
        foo(())

    ###################

    @typecheck
    def foo(x: list_of((by_regex("^[01]+$"), int))) -> bool:
        return functools.reduce(lambda r, e: r and int(e[0], 2) == e[1],
                                x, True)

    assert foo([("1010", 10), ("0101", 5)])
    assert not foo([("1010", 10), ("0111", 77)])

    ###################

    assert list_of(optional(by_regex("^foo$")))(["foo", None, "foo"]) and \
           list_of(optional(by_regex("^foo$"))).check(["foo", None, "foo"])

    assert not list_of(optional(by_regex("^foo$")))(["123", None, "foo"]) and \
           not list_of(optional(by_regex("^foo$"))).check(["123", None, "foo"])

    ###################

    print("ok")

    ############################################################################

    print("dict_of: ", end = "")

    ###################

    @typecheck
    def foo(x: dict_of(int, str)) -> dict_of(str, int):
        return { v: k for k, v in x.items() }

    assert foo({}) == {}
    assert foo({1: "1", 2: "2"}) == {"1": 1, "2": 2}

    with expected(InputParameterError("foo() has got an incompatible value for x: []")):
        foo([])

    with expected(InputParameterError("foo() has got an incompatible value for x: {'1': '2'}")):
        foo({"1": "2"})

    with expected(InputParameterError("foo() has got an incompatible value for x: {1: 2}")):
        foo({1: 2})

    ###################

    @typecheck
    def foo(*, k: dict_of((int, int), [by_regex("^[0-9]+$"), by_regex("^[0-9]+$")])):
        return functools.reduce(lambda r, t: r and str(t[0][0]) == t[1][0] and str(t[0][1]) == t[1][1],
                                k.items(), True)

    assert foo(k = { (1, 2): ["1", "2"], (3, 4): ["3", "4"]})
    assert not foo(k = { (1, 3): ["1", "2"], (3, 4): ["3", "4"]})
    assert not foo(k = { (1, 2): ["1", "2"], (3, 4): ["3", "5"]})

    with expected(InputParameterError("foo() has got an incompatible value for k: {(1, 2): ['1', '2'], (3, 4): ['3', 'x']}")):
        foo(k = { (1, 2): ["1", "2"], (3, 4): ["3", "x"]})

    with expected(InputParameterError("foo() has got an incompatible value for k: {(1, 2): ['1', '2'], (3,): ['3', '4']}")):
        foo(k = { (1, 2): ["1", "2"], (3, ): ["3", "4"]})

    with expected(InputParameterError("foo() has got an incompatible value for k: {(1, 2): ['1', '2'], (3, 4.0): ['3', '4']}")):
        foo(k = { (1, 2): ["1", "2"], (3, 4.0): ["3", "4"]})

    ###################

    assert dict_of(int, optional(str))({ 1: "foo", 2: None }) and \
           dict_of(int, optional(str)).check({ 1: "foo", 2: None })

    assert not dict_of(int, optional(str))({ None: "foo", 2: None }) and \
           not dict_of(int, optional(str)).check({ None: "foo", 2: None })

    ###################

    print("ok")

    ############################################################################

    print("one_of: ", end = "")

    ###################

    @typecheck
    def foo(x: one_of(int, 1)) -> one_of(1, int):
        return x

    assert foo(1) == 1
    assert foo(int) is int

    with expected(InputParameterError("foo() has got an incompatible value for x: 2")):
        foo(2)

    @typecheck
    def bar(*, x: one_of(None)) -> one_of():
        return x

    with expected(ReturnValueError("bar() has returned an incompatible value: None")):
        bar(x = None)

    with expected(TypeCheckSpecificationError("the default value for x is incompatible with its typecheck")):
        @typecheck
        def foo(x: one_of(1) = 2):
            pass

    @typecheck
    def foo(x: optional(one_of(1, 2)) = 2):
        return x

    assert foo() == 2

    ###################

    print("ok")

    ############################################################################

    print("either: ", end = "")

    ###################

    @typecheck
    def foo(x: either()):
        pass

    with expected(InputParameterError("foo() has got an incompatible value for x: 1")):
        foo(1)

    @typecheck
    def bar(x: either((int, float), by_regex("^foo$"), one_of(b"X", b"Y"))):
        pass

    bar((1, 1.0))
    bar("foo")
    bar(b"X")
    bar(b"Y")

    with expected(InputParameterError("bar() has got an incompatible value for x: (1.0, 1)")):
        bar((1.0, 1))
    with expected(InputParameterError("bar() has got an incompatible value for x: b'foo'")):
        bar(b"foo")
    with expected(InputParameterError("bar() has got an incompatible value for x: X")):
        bar("X")
    with expected(InputParameterError("bar() has got an incompatible value for x: Y")):
        bar("Y")

    nothing_at_all = ((nothing, ) * 1000)
    either_nothing = either(either(either(either(*nothing_at_all), *nothing_at_all), *nothing_at_all), *nothing_at_all)

    @typecheck
    def biz(x) -> either_nothing:
        return x

    with expected(ReturnValueError("biz() has returned an incompatible value: anything")):
        biz("anything")

    @typecheck
    def accept_number(x: either(int, by_regex("^[0-9]+$"))):
        return int(x) + 1

    assert accept_number(1) == 2
    assert accept_number("1") == 2
    assert accept_number(-1) == 0
    with expected(InputParameterError("accept_number() has got an incompatible value for x: -1")):
        accept_number("-1")

    ###################

    print("ok")

    ############################################################################

    print("custom exceptions: ", end = "")

    ###################

    @typecheck_with_exceptions(input_parameter_error = ZeroDivisionError)
    def foo(x: int):
        pass

    with expected(ZeroDivisionError("foo() has got an incompatible value for x: 1")):
        foo("1")

    @typecheck_with_exceptions(return_value_error = MemoryError)
    def foo(x) -> str:
        return x

    with expected(MemoryError):
        foo(1)

    @typecheck_with_exceptions(input_parameter_error = TypeError, return_value_error = TypeError)
    def foo(x: int) -> int:
        return x

    assert foo(1) == 1

    with expected(InputParameterError("typecheck_with_exceptions() has got an incompatible "
                                      "value for input_parameter_error: <class 'int'>")):
        @typecheck_with_exceptions(input_parameter_error = int)
        def foo():
            pass

    with expected(InputParameterError("typecheck_with_exceptions() has got an incompatible "
                                      "value for return_value_error: <class 'int'>")):
        @typecheck_with_exceptions(return_value_error = int)
        def foo():
            pass

    ###################

    print("ok")

    ############################################################################

    print("disable: ", end = "")

    ###################

    @typecheck
    def foo(x: int):
        pass

    disable() # disable-only switch, no further proxying is performed

    @typecheck
    def bar(x: int):
        pass

    foo(1)
    with expected(InputParameterError("foo() has got an incompatible value for x: 1")):
        foo("1")

    bar(1)
    bar("1")

    ###################

    print("ok")

    ############################################################################

    print("all ok")

################################################################################
# EOF