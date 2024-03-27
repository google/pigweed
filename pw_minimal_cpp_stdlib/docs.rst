.. _module-pw_minimal_cpp_stdlib:

=====================
pw_minimal_cpp_stdlib
=====================
.. admonition:: 🛑 Stop 🛑

   **Do not use this module** unless you have consulted with the Pigweed team.

-----------
Maintenance
-----------
If you discovered this module because of a build failure in a
``pw_strict_host_clang_size_optimized_minimal_cpp_stdlib`` or similar target,
the important thing to know is that a small set of Pigweed targets are expected
to compile using a limited set of C++ Standard Library features and headers
that have been polyfilled by this module in order to support environments
without a C++ stdlib.

Failed ``#include`` lines should be solved by either eliminating the dependency
from the target (strongly preferred) or by adding a (minimal) polyfill of the
relevant C++ header to this module.

Polyfills may assume a C++17 (or greater) compiler but with only a C (not C++)
stdlib. As this target does not support tests, polyfills can only be checked for
compilation; this can be done by building the root
``build_with_pw_minimal_cpp_stdlib group``
(e.g. ``ninja -C out build_with_pw_minimal_cpp_stdlib``).

-------
Purpose
-------
The ``pw_minimal_cpp_stdlib`` module provides an extremely limited, incomplete
implementation of the C++ Standard Library. This module is only intended for
testing and development when compiling with C++17 or newer, but without access
to the C++ Standard Library (``-nostdinc++``).

Production code should use a real C++ Standard Library implementation, such as
`libc++ <https://libcxx.llvm.org/>`_ or `libstdc++
<https://gcc.gnu.org/onlinedocs/libstdc++/>`_.

.. warning::

  ``pw_minimal_cpp_stdlib`` was created for a very specific purpose. It is NOT a
  general purpose C++ Standard Library implementation and should not be used as
  one.

  - Many library features are **missing**.
  - Many features are **non-functioning stubs**.
  - Some features **do not match the C++ standard**.
  - Test coverage is **extremely limited**.

-----------------
Build integration
-----------------
The top-level ``build_with_minimal_cpp_stdlib`` GN group builds a few supported
modules with ``pw_minimal_cpp_stdlib`` swapped in for the C++ library at the
toolchain level. Notably, ``pw_minimal_cpp_stdlib`` does not support
``pw_unit_test``, so this group does NOT run any tests.

-----------
Code layout
-----------
The C++ Standard Library headers (e.g. ``<cstdint>`` and ``<type_traits>``) are
defined in ``public/``. These files are symlinks to their implementations in
``public/internal/``.

.. tip::

  You can automatically recreate the symlinks in ``public/`` by executing the
  following Bash code from ``pw_minimal_cpp_stdlib/public/``.

  .. code-block:: bash

     for f in $(ls pw_minimal_cpp_stdlib/internal/); do ln -s pw_minimal_cpp_stdlib/internal/$f ${f%.h}; done

------------
Requirements
------------
- C++17
- gcc or clang
- The C Standard Library
