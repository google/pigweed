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
``"$dir_pw_polyfill"`` libraries, which the GN build adds as a
dependency for all targets:

* ``$dir_pw_polyfill:bit``
* ``$dir_pw_polyfill:cstddef``
* ``$dir_pw_polyfill:iterator``
* ``$dir_pw_polyfill:span``
* ``$dir_pw_polyfill:type_traits``

To apply overrides in Bazel or CMake, depend on the targets you need such as
``//pw_polyfill:span`` or ``pw_polyfill.span`` as an example.

Backported features
===================
==================  ================================  ===============================  ========================================
Header              Feature                           Level of support                 Feature test macro
==================  ================================  ===============================  ========================================
<bit>               std::endian                       full                             __cpp_lib_endian
<cstdlib>           std::byte                         full                             __cpp_lib_byte
<iterator>          std::data, std::size              full                             __cpp_lib_nonmember_container_access
<type_traits>       std::bool_constant                full                             __cpp_lib_bool_constant
<type_traits>       std::negation, etc.               full                             __cpp_lib_logical_traits
==================  ================================  ===============================  ========================================

----------------------------------------------------
Adapt code to compile with different versions of C++
----------------------------------------------------
 ``pw_polyfill`` provides features for adapting to different C++ standards when
 ``pw_polyfill:overrides``'s automatic backporting is insufficient:

  - ``pw_polyfill/standard.h`` -- provides a macro for checking the C++ standard
  - ``pw_polyfill/language_feature_macros.h`` -- provides macros for adapting
    code to work with or without newer language features

Language feature macros
=======================
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
