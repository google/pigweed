.. _chapter-pw-polyfill:

.. default-domain:: cpp

.. highlight:: sh

-----------
pw_polyfill
-----------
The ``pw_polyfill`` module backports C++17 features to C++11 and C++14.

Compatibility
=============
C++11

Features
========

Adapt code to compile with older versions of C++
------------------------------------------------
The ``pw_polyfill`` module provides features for adapting C++17 code to work
when compiled with older C++ standards.

  - ``pw_polyfill/standard.h`` -- provides a macro for checking the C++ standard
  - ``pw_polyfill/language_features.h`` -- provides macros for adapting code to
    work without newer language features
  - ``pw_polyfill/standard_library/`` -- adapters for C++ standard library
    features, such as ``std::byte``, ``std::size``/``std::data``, and type
    traits convenience aliases

In GN, Bazel, or CMake, depend on ``$dir_pw_polyfill``, ``//pw_polyfill``,
or ``pw_polyfill``, respectively. In other build systems, add
``pw_polyfill/standard_library_public`` and ``pw_polyfill/public_overrides`` as
include paths.

Override C++ standard library headers
-------------------------------------
The headers in ``public_overrides`` provide wrappers for C++ standard library
headers, including ``<cstddef>``, ``<iterator>``, ``<type_traits>``. These are
provided through the ``"$dir_pw_polyfill:overrides"`` library, which the GN
build adds as a dependency for all targets. This makes some C++17 library
features available to targets compiled with older C++ standards, without needing
to change the code.

To apply overrides in Bazel or CMake, depend on the
``//pw_polyfill:overrides`` or ``pw_polyfill.overrides`` targets. In other build
systems, add ``pw_polyfill/standard_library_public`` and
``pw_polyfill/public_overrides`` as include paths.
