.. _chapter-pw-preprocessor:

.. default-domain:: cpp

.. highlight:: sh

---------------
pw_preprocessor
---------------
The preprocessor module provides various helpful preprocessor macros.

Compatibility
=============
C and C++

Headers
=======
The preprocessor module provides several headers.

pw_preprocessor/boolean.h
-------------------------
Defines macros for boolean logic on literal 1s and 0s. This is useful for
situations when a literal is needed to build the name of a function or macro.

pw_preprocessor/compiler.h
--------------------------
Macros for compiler-specific features, such as attributes or builtins.

pw_preprocessor/concat.h
------------------------
Defines the ``PW_CONCAT(...)`` macro, which expands its arguments if they are
macros and token pastes the results. This can be used for building names of
classes, variables, macros, etc.

pw_preprocessor/macro_arg_count.h
---------------------------------
Defines the ``PW_ARG_COUNT(...)`` macro, which counts the number of arguments it
was passed. It can be invoked directly or with ``__VA_ARGS__`` in another macro.
``PW_ARG_COUNT(...)``  evaluates to a literal of the number of arguments which
can be used directly or concatenated to build other names. Unlike many common
implementations, this macro correctly evaluates to ``0`` when it is invoked
without arguments.

This header also defines ``PW_HAS_ARGS(...)`` and ``PW_HAS_NO_ARGS(...)``,
which evaluate to ``1`` or ``0`` depending on whether they are invoked with
arguments.

pw_preprocessor/util.h
----------------------
General purpose, useful macros.

* ``PW_ARRAY_SIZE(array)`` -- calculates the size of a C array
* ``PW_UNUSED(value)`` -- silences "unused variable" compiler warnings
* ``PW_STRINGIFY(...)`` -- expands its arguments as macros and converts them to
  a string literal
* ``PW_EXTERN_C`` -- declares a name to be ``extern "C"`` in C++; expands to
  nothing in C
* ``PW_EXTERN_C_START`` / ``PW_EXTERN_C_END`` -- declares an ``extern "C" { }``
  block in C++; expands to nothing in C
