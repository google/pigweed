.. _module-pw_minimal_cpp_stdlib:

---------------------
pw_minimal_cpp_stdlib
---------------------
The ``pw_minimal_cpp_stdlib`` module provides an extremely limited
implementation of the C++ Standard Library. This module falls far, far short of
providing a complete C++ Standard Library and should only be used in dire
situations where you happen to be compiling with C++17 but don't have a C++
Standard Library available to you.

The C++ Standard Library headers (e.g. ``<cstdint>`` and ``<type_traits>``) are
defined in ``public/``. These files are symlinks to their implementations in
``public/internal/``.

.. tip::

  You can automatically recreate the symlinks in ``public/`` by executing the
  following Bash code from ``pw_minimal_cpp_stdlib/public/``.

  .. code-block:: bash

    for f in $(ls internal/); do ln -s internal/$f ${f%.h}; done

Requirements
============
- C++17
- gcc or clang
- The C Standard Library
