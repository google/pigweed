.. _module-pw_polyfill:

===========
pw_polyfill
===========
The ``pw_polyfill`` module backports new C++ features to C++14.

------------------------------------------------
Backport new C++ features to older C++ standards
------------------------------------------------
The main purpose of ``pw_polyfill`` is to bring new C++ library and language
features to older C++ standards. No additional ``#include`` statements are
required to use these features; simply write code assuming that the features are
available. This implicit feature backporting is provided through the
``overrides`` library in the ``pw_polyfill`` module. GN automatically adds this
library as a dependency in ``pw_source_set``.

``pw_polyfill`` backports C++ library features by wrapping the standard C++ and
C headers. The wrapper headers include the original header using
`#include_next <https://gcc.gnu.org/onlinedocs/cpp/Wrapper-Headers.html>`_, then
add missing features. The backported features are only defined if they aren't
provided by the standard header, so ``pw_polyfill`` is safe to use when
compiling with any standard C++14 or newer.

The wrapper headers are in ``public_overrides``. These are provided through the
following libraries:

* ``pw_polyfill:cstddef``
* ``pw_polyfill:iterator``
* ``pw_polyfill:span``

The GN build automatically adds dependencies on the ``<span>`` header. To apply
other overrides, add dependencies as needed. In Bazel or CMake, overrides are
not applied automatically, so depend on the targets you need such as
``//pw_polyfill:span`` or ``pw_polyfill.span``.

Backported features
===================

.. list-table::

  * - **Header**
    - **Feature**
    - **Feature test macro**
    - **Module**
    - **Polyfill header**
    - **Polyfill name**
  * - ``<array>``
    - ``std::to_array``
    - ``__cpp_lib_to_array``
    - :ref:`module-pw_containers`
    - ``pw_containers/to_array.h``
    - ``pw::containers::to_array``
  * - ``<bit>``
    - ``std::endian``
    - ``__cpp_lib_endian``
    - :ref:`module-pw_bytes`
    - ``pw_bytes/bit.h``
    - ``pw::endian``
  * - ``<cstdlib>``
    - ``std::byte``
    - ``__cpp_lib_byte``
    - pw_polyfill
    - ``<cstdlib>``
    - ``std::byte``
  * - ``<iterator>``
    - ``std::data``, ``std::size``
    - ``__cpp_lib_nonmember_container_access``
    - pw_polyfill
    - ``<iterator>``
    - ``std::data``, ``std::size``
  * - ``<span>``
    - ``std::span``
    - ``__cpp_lib_span``
    - pw_polyfill
    - ``<span>``
    - ``std::span``

----------------------------------------------------
Adapt code to compile with different versions of C++
----------------------------------------------------
``pw_polyfill`` provides features for adapting to different C++ standards when
``pw_polyfill:overrides``'s automatic backporting is insufficient.

C++ standard macro
==================
``pw_polyfill/standard.h`` provides a macro for checking if a C++ standard is
supported.

.. c:macro:: PW_CXX_STANDARD_IS_SUPPORTED(standard)

   Evaluates true if the provided C++ standard (98, 11, 14, 17, 20) is supported
   by the compiler. This is a simpler, cleaner alternative to checking the value
   of the ``__cplusplus`` macro.

Language feature macros
=======================
``pw_polyfill/language_feature_macros.h`` provides macros for adapting code to
work with or without newer language features

======================  ================================  ========================================  ==========================
Macro                   Feature                           Description                               Feature test macro
======================  ================================  ========================================  ==========================
PW_INLINE_VARIABLE      inline variables                  inline if supported by the compiler       __cpp_inline_variables
PW_CONSTEXPR_CPP20      constexpr in C++20                constexpr if compiling for C++20          __cplusplus >= 202002L
PW_CONSTEVAL            consteval                         consteval if supported by the compiler    __cpp_consteval
PW_CONSTINIT            constinit                         constinit in clang and GCC 10+            __cpp_constinit
======================  ================================  ========================================  ==========================

In GN, Bazel, or CMake, depend on ``$dir_pw_polyfill``, ``//pw_polyfill``,
or ``pw_polyfill``, respectively, to access these features. In other build
systems, add ``pw_polyfill/standard_library_public`` and
``pw_polyfill/public_overrides`` as include paths.

-------------
Compatibility
-------------
C++14

Zephyr
======
To enable ``pw_polyfill`` for Zephyr add ``CONFIG_PIGWEED_POLYFILL=y`` to the
project's configuration. Similarly, to enable ``pw_polyfill.overrides``, add
``CONFIG_PIGWEED_POLYFILL_OVERRIDES=y`` to the project's configuration.
