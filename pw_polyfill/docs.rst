.. _module-pw_polyfill:

===========
pw_polyfill
===========
The ``pw_polyfill`` module supports compiling code against different C++
standards.

--------------------------------------------------
Adapt code to compile with different C++ standards
--------------------------------------------------

C++ standard macro
==================
``pw_polyfill/standard.h`` provides a macro for checking if a C++ standard is
supported.

.. c:macro:: PW_CXX_STANDARD_IS_SUPPORTED(standard)

   Evaluates true if the provided C++ standard (98, 11, 14, 17, 20, 23) is
   supported by the compiler. This is a simpler, cleaner alternative to checking
   the value of the ``__cplusplus`` macro.

Language feature macros
=======================
``pw_polyfill/language_feature_macros.h`` provides macros for adapting code to
work with or without C++ language features.

.. list-table::
  :header-rows: 1

  * - Macro
    - Feature
    - Description
    - Feature test macro
  * - ``PW_CONSTEXPR_CPP20``
    - ``constexpr``
    - ``constexpr`` if compiling for C++20
    - ``__cplusplus >= 202002L``
  * - ``PW_CONSTEVAL``
    - ``consteval``
    - ``consteval`` if supported by the compiler
    - ``__cpp_consteval``
  * - ``PW_CONSTINIT``
    - ``constinit``
    - ``constinit`` in clang and GCC 10+
    - ``__cpp_constinit``

In GN, Bazel, or CMake, depend on ``$dir_pw_polyfill``, ``//pw_polyfill``,
or ``pw_polyfill``, respectively, to access these features. In other build
systems, add ``pw_polyfill/public`` as an include path.

------------------------------------------------
Backport new C++ features to older C++ standards
------------------------------------------------
Pigweed backports a few C++ features to older C++ standards. These features
are provided in the ``pw`` namespace. If the features are provided by the
toolchain, the ``pw`` versions are aliases of the ``std`` versions.

These features are documented here, but are not implemented in ``pw_polyfill``.

Backported features
===================
.. list-table::
  :header-rows: 1

  * - Header
    - Feature
    - Feature test macro
    - Module
    - Polyfill header
    - Polyfill name
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
  * - ``<expected>``
    - ``std::expected``
    - ``__cpp_lib_expected``
    - :ref:`module-pw_result`
    - ``pw_result/expected.h``
    - ``pw::expected``
  * - ``<span>``
    - ``std::span``
    - ``__cpp_lib_span``
    - :ref:`module-pw_span`
    - ``pw_span/span.h``
    - ``pw::span``

.. _module-pw_polyfill-static_assert:

------------------------------------------------
Adapt code to compile with different C standards
------------------------------------------------
``pw_polyfill/static_assert.h`` provides C23-style ``static_assert`` for older
C standards. As in C23, the message argument is optional.

.. literalinclude:: c_test.c
   :start-after: [static_assert-example]
   :end-before: [static_assert-example]
   :language: c

.. tip::

   Use ``pw_polyfill/static_assert.h`` rather than ``<assert.h>`` to provide
   ``static_assert`` in C. ``<assert.h>`` provides a ``#define static_assert
   _Static_assert`` macro prior to C23, but its ``static_assert`` does not
   support C23 semantics (optional message argument), and it defines the
   unwanted ``assert`` macro.

------
Zephyr
------
To enable ``pw_polyfill`` for Zephyr add ``CONFIG_PIGWEED_POLYFILL=y`` to the
project's configuration.
