.. _module-pw_polyfill:

===========
pw_polyfill
===========
The ``pw_polyfill`` module backports new C++ features to older C++ standards.
When possible, features are adapted to work with in standards as old as C++11.
Pigweed does not support C++ standards older than C++11.

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
compiling with any standard C++11 or newer.

Language features are backported or stubbed via the
``pw_polyfill/language_features.h`` header. This header is included in files
that depend on ``pw_polyfill`` with GCC's ``-include`` option so that no
``#include`` statement is required.

The wrapper headers are in ``public_overrides``. These are provided through the
``"$dir_pw_polyfill:overrides"`` library, which the GN build adds as a
dependency for all targets. To apply overrides in Bazel or CMake, depend on the
``//pw_polyfill:overrides`` or ``pw_polyfill.overrides`` targets. In other build
systems, add ``pw_polyfill/standard_library_public`` and
``pw_polyfill/public_overrides`` as include paths, and add a ``-include`` option
for the ``language_features.h`` header.

Backported features
===================
==================  ================================  ============================================  ========================================
Header              Feature                           Level of support                              Feature test macro
==================  ================================  ============================================  ========================================
<array>             std::to_array                     full                                          __cpp_lib_to_array
<bit>               std::endian                       full                                          __cpp_lib_endian
<cstdlib>           std::byte                         full; some operators not constexpr in C++11   __cpp_lib_byte
<iterator>          std::data, std::size              full                                          __cpp_lib_nonmember_container_access
<type_traits>       \*_t trait aliases                partial (can expand as needed)                __cpp_lib_transformation_trait_aliases
<type_traits>       std::is_null_pointer              full                                          __cpp_lib_is_null_pointer
<utilty>            std::integer_sequence & helpers   full                                          __cpp_lib_integer_sequence
(language feature)  consteval keyword                 ignored (equivalent to constexpr)             __cpp_consteval
(language feature)  constinit keyword                 supported in clang; ignored in GCC            __cpp_constinit
(language feature)  static_assert with no message     full                                          __cpp_static_assert
==================  ================================  ============================================  ========================================

----------------------------------------------------
Adapt code to compile with different versions of C++
----------------------------------------------------
 ``pw_polyfill`` provides features for adapting to different C++ standards when
 ``pw_polyfill:overrides``'s automatic backporting is insufficient:

  - ``pw_polyfill/standard.h`` -- provides a macro for checking the C++ standard
  - ``pw_polyfill/language_feature_macros.h`` -- provides macros for adapting
    code to work with or without newer language features

In GN, Bazel, or CMake, depend on ``$dir_pw_polyfill``, ``//pw_polyfill``,
or ``pw_polyfill``, respectively, to access these features. In other build
systems, add ``pw_polyfill/standard_library_public`` and
``pw_polyfill/public_overrides`` as include paths.

-------------
Compatibility
-------------
C++11
