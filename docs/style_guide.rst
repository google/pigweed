.. _docs-pw-style:

===========
Style Guide
===========
- :ref:`cpp-style`
- :ref:`owners-style`
- :ref:`python-style`
- :ref:`documentation-style`
- :ref:`commit-style`

.. tip::
   Pigweed runs ``pw format`` as part of ``pw presubmit`` to perform some code
   formatting checks. To speed up the review process, consider adding ``pw
   presubmit`` as a git push hook using the following command:
   ``pw presubmit --install``

.. _cpp-style:

---------
C++ style
---------
The Pigweed C++ style guide is closely based on Google's external C++ Style
Guide, which is found on the web at
https://google.github.io/styleguide/cppguide.html. The Google C++ Style Guide
applies to Pigweed except as described in this document.

The Pigweed style guide only applies to Pigweed itself. It does not apply to
projects that use Pigweed or to the third-party code included with Pigweed.
Non-Pigweed code is free to use features restricted by Pigweed, such as dynamic
memory allocation and the entirety of the C++ Standard Library.

Recommendations in the :doc:`embedded_cpp_guide` are considered part of the
Pigweed style guide, but are separated out since it covers more general
embedded development beyond just C++ style.

C++ standard
============
All Pigweed C++ code must compile with ``-std=c++17`` in Clang and GCC. C++20
features may be used as long as the code still compiles unmodified with C++17.
See ``pw_polyfill/language_feature_macros.h`` for macros that provide C++20
features when supported.

Compiler extensions should not be used unless wrapped in a macro or properly
guarded in the preprocessor. See ``pw_processor/compiler.h`` for macros that
wrap compiler-specific features.

Automatic formatting
====================
Pigweed uses `clang-format <https://clang.llvm.org/docs/ClangFormat.html>`_ to
automatically format Pigweed source code. A ``.clang-format`` configuration is
provided with the Pigweed repository.

Automatic formatting is essential to facilitate large-scale, automated changes
in Pigweed. Therefore, all code in Pigweed is expected to be formatted with
``clang-format`` prior to submission. Existing code may be reformatted at any
time.

If ``clang-format`` formats code in an undesirable or incorrect way, it can be
disabled for the affected lines by adding ``// clang-format off``.
``clang-format`` must then be re-enabled with a ``// clang-format on`` comment.

.. code-block:: cpp

  // clang-format off
  constexpr int kMyMatrix[] = {
      100,  23,   0,
        0, 542,  38,
        1,   2, 201,
  };
  // clang-format on

C Standard Library
==================
In C++ headers, always use the C++ versions of C Standard Library headers (e.g.
``<cstdlib>`` instead of ``<stdlib.h>``). If the header is used by both C and
C++ code, only the C header should be used.

In C++ code, it is preferred to use C functions from the ``std`` namespace. For
example, use ``std::memcpy`` instead of ``memcpy``. The C++ standard does not
require the global namespace versions of the functions to be provided. Using
``std::`` is more consistent with the C++ Standard Library and makes it easier
to distinguish Pigweed functions from library functions.

Within core Pigweed, do not use C standard library functions that allocate
memory, such as ``std::malloc``. There are exceptions to this for when dynamic
allocation is enabled for a system; Pigweed modules are allowed to add extra
functionality when a heap is present; but this must be optional.

C++ Standard Library
====================
Much of the C++ Standard Library is not a good fit for embedded software. Many
of the classes and functions were not designed with the RAM, flash, and
performance constraints of a microcontroller in mind. For example, simply
adding the line ``#include <iostream>`` can increase the binary size by 150 KB!
This is larger than many microcontrollers' entire internal storage.

However, with appropriate caution, a limited set of standard C++ libraries can
be used to great effect. Developers can leverage familiar, well-tested
abstractions instead of writing their own. C++ library algorithms and classes
can give equivalent or better performance than hand-written C code.

A limited subset of the C++ Standard Library is permitted in Pigweed. To keep
Pigweed small, flexible, and portable, functions that allocate dynamic memory
must be avoided. Care must be exercised when using multiple instantiations of a
template function, which can lead to code bloat.

Permitted Headers
-----------------
.. admonition:: The following C++ Standard Library headers are always permitted:
   :class: checkmark

   * ``<array>``
   * ``<complex>``
   * ``<initializer_list>``
   * ``<iterator>``
   * ``<limits>``
   * ``<optional>``
   * ``<random>``
   * ``<ratio>``
   * ``<string_view>``
   * ``<tuple>``
   * ``<type_traits>``
   * ``<utility>``
   * ``<variant>``
   * C Standard Library headers (``<c*>``)

.. admonition:: With caution, parts of the following headers can be used:
   :class: warning

   * ``<algorithm>`` -- be wary of potential memory allocation
   * ``<atomic>`` -- not all MCUs natively support atomic operations
   * ``<bitset>`` -- conversions to or from strings are disallowed
   * ``<functional>`` -- do **not** use ``std::function``; use
     :ref:`module-pw_function`
   * ``<mutex>`` -- can use ``std::lock_guard``, use :ref:`module-pw_sync` for
     mutexes
   * ``<new>`` -- for placement new
   * ``<numeric>`` -- be wary of code size with multiple template instantiations

.. admonition:: Never use any of these headers:
   :class: error

   * Dynamic containers (``<list>``, ``<map>``, ``<set>``, ``<vector>``, etc.)
   * Streams (``<iostream>``, ``<ostream>``, ``<fstream>``, ``<sstream>`` etc.)
     -- in some cases :ref:`module-pw_stream` can work instead
   * ``<span>`` -- use :ref:`module-pw_span` instead. Downstream projects may
     want to directly use ``std::span`` if it is available, but upstream must
     use the ``pw::span`` version for compatability
   * ``<string>`` -- can use :ref:`module-pw_string`
   * ``<thread>`` -- can use :ref:`module-pw_thread`
   * ``<future>`` -- eventually :ref:`module-pw_async` will offer this
   * ``<exception>``, ``<stdexcept>`` -- no exceptions
   * ``<memory>``, ``<scoped_allocator>`` -- no allocations
   * ``<regex>``
   * ``<valarray>``

Headers not listed here should be carefully evaluated before they are used.

These restrictions do not apply to third party code or to projects that use
Pigweed.

Combining C and C++
===================
Prefer to write C++ code over C code, using ``extern "C"`` for symbols that must
have C linkage. ``extern "C"`` functions should be defined within C++
namespaces to simplify referring to other code.

C++ functions with no parameters do not include ``void`` in the parameter list.
C functions with no parameters must include ``void``.

.. code-block:: cpp

  namespace pw {

  bool ThisIsACppFunction() { return true; }

  extern "C" int pw_ThisIsACFunction(void) { return -1; }

  extern "C" {

  int pw_ThisIsAlsoACFunction(void) {
    return ThisIsACppFunction() ? 100 : 0;
  }

  }  // extern "C"

  }  // namespace pw

Comments
========
Prefer C++-style (``//``) comments over C-style comments (``/* */``). C-style
comments should only be used for inline comments.

.. code-block:: cpp

  // Use C++-style comments, except where C-style comments are necessary.
  // This returns a random number using an algorithm I found on the internet.
  #define RANDOM_NUMBER() [] {                \
    return 4;  /* chosen by fair dice roll */ \
  }()

Indent code in comments with two additional spaces, making a total of three
spaces after the ``//``. All code blocks must begin and end with an empty
comment line, even if the blank comment line is the last line in the block.

.. code-block:: cpp

  // Here is an example of code in comments.
  //
  //   int indentation_spaces = 2;
  //   int total_spaces = 3;
  //
  //   engine_1.thrust = RANDOM_NUMBER() * indentation_spaces + total_spaces;
  //
  bool SomeFunction();

Control statements
==================

Loops and conditionals
----------------------
All loops and conditional statements must use braces, and be on their own line.

.. admonition:: **Yes**: Always use braces for line conditionals and loops:
   :class: checkmark

   .. code:: cpp

      while (SomeCondition()) {
        x += 2;
      }
      if (OtherCondition()) {
        DoTheThing();
      }


.. admonition:: **No**: Missing braces
   :class: error

   .. code:: cpp

      while (SomeCondition())
        x += 2;
      if (OtherCondition())
        DoTheThing();

.. admonition:: **No**: Statement on same line as condition
   :class: error

   .. code:: cpp

      while (SomeCondition()) { x += 2; }
      if (OtherCondition()) { DoTheThing(); }


The syntax ``while (true)`` is preferred over ``for (;;)`` for infinite loops.

.. admonition:: **Yes**:
   :class: checkmark

   .. code:: cpp

      while (true) {
        DoSomethingForever();
      }

.. admonition:: **No**:
   :class: error

   .. code:: cpp

      for (;;) {
        DoSomethingForever();
      }


Prefer early exit with ``return`` and ``continue``
--------------------------------------------------
Prefer to exit early from functions and loops to simplify code. This is the
same same conventions as `LLVM
<https://llvm.org/docs/CodingStandards.html#use-early-exits-and-continue-to-simplify-code>`_.
We find this approach is superior to the "one return per function" style for a
multitude of reasons:

* **Visually**, the code is easier to follow, and takes less horizontal screen
  space.
* It makes it clear what part of the code is the **"main business" versus "edge
  case handling"**.
* For **functions**, parameter checking is in its own section at the top of the
  function, rather than scattered around in the fuction body.
* For **loops**, element checking is in its own section at the top of the loop,
  rather than scattered around in the loop body.
* Commit **deltas are simpler to follow** in code reviews; since adding a new
  parameter check or loop element condition doesn't cause an indentation change
  in the rest of the function.

The guidance applies in two cases:

* **Function early exit** - Early exits are for function parameter checking
  and edge case checking at the top. The main functionality follows.
* **Loop early exit** - Early exits in loops are for skipping an iteration
  due to some edge case with an item getting iterated over. Loops may also
  contain function exits, which should be structured the same way (see example
  below).

.. admonition:: **Yes**: Exit early from functions; keeping the main handling
   at the bottom and de-dentend.
   :class: checkmark

   .. code:: cpp

      Status DoSomething(Parameter parameter) {
        // Parameter validation first; detecting incoming use errors.
        PW_CHECK_INT_EQ(parameter.property(), 3, "Programmer error: frobnitz");

        // Error case: Not in correct state.
        if (parameter.other() == MyEnum::kBrokenState) {
          LOG_ERROR("Device in strange state: %s", parametr.state_str());
          return Status::InvalidPrecondition();
        }

        // Error case: Not in low power mode; shouldn't do anything.
        if (parameter.power() != MyEnum::kLowPower) {
          LOG_ERROR("Not in low power mode");
          return Status::InvalidPrecondition();
        }

        // Main business for the function here.
        MainBody();
        MoreMainBodyStuff();
      }

.. admonition:: **No**: Main body of function is buried and right creeping.
   Even though this is shorter than the version preferred by Pigweed due to
   factoring the return statement, the logical structure is less obvious. A
   function in Pigweed containing **nested conditionals indicates that
   something complicated is happening with the flow**; otherwise it would have
   the early bail structure; so pay close attention.
   :class: error

   .. code:: cpp

      Status DoSomething(Parameter parameter) {
        // Parameter validation first; detecting incoming use errors.
        PW_CHECK_INT_EQ(parameter.property(), 3, "Programmer error: frobnitz");

        // Error case: Not in correct state.
        if (parameter.other() != MyEnum::kBrokenState) {
          // Error case: Not in low power mode; shouldn't do anything.
          if (parameter.power() == MyEnum::kLowPower) {
            // Main business for the function here.
            MainBody();
            MoreMainBodyStuff();
          } else {
            LOG_ERROR("Not in low power mode");
          }
        } else {
          LOG_ERROR("Device in strange state: %s", parametr.state_str());
        }
        return Status::InvalidPrecondition();
      }

.. admonition:: **Yes**: Bail early from loops; keeping the main handling at
   the bottom and de-dentend.
   :class: checkmark

   .. code:: cpp

      for (int i = 0; i < LoopSize(); ++i) {
        // Early skip of item based on edge condition.
        if (!CommonCase()) {
          continue;
        }
        // Early exit of function based on error case.
        int my_measurement = GetSomeMeasurement();
        if (my_measurement < 10) {
          LOG_ERROR("Found something strange; bailing");
          return Status::Unknown();
        }

        // Main body of the loop.
        ProcessItem(my_items[i], my_measurement);
        ProcessItemMore(my_items[i], my_measurement, other_details);
        ...
      }

.. admonition:: **No**: Right-creeping code with the main body buried inside
   some nested conditional. This makes it harder to understand what is the
   main purpose of the loop versus what is edge case handling.
   :class: error

   .. code:: cpp

      for (int i = 0; i < LoopSize(); ++i) {
        if (CommonCase()) {
          int my_measurement = GetSomeMeasurement();
          if (my_measurement >= 10) {
            // Main body of the loop.
            ProcessItem(my_items[i], my_measurement);
            ProcessItemMore(my_items[i], my_measurement, other_details);
            ...
          } else {
            LOG_ERROR("Found something strange; bailing");
            return Status::Unknown();
          }
        }
      }

There are cases where this structure doesn't work, and in those cases, it is
fine to structure the code differently.

No ``else`` after ``return`` or ``continue``
--------------------------------------------
Do not put unnecessary ``} else {`` blocks after blocks that terminate with a
return, since this causes unnecessary rightward indentation creep. This
guidance pairs with the preference for early exits to reduce code duplication
and standardize loop/function structure.

.. admonition:: **Yes**: No else after return or continue
   :class: checkmark

   .. code:: cpp

      // Note lack of else block due to return.
      if (Failure()) {
        DoTheThing();
        return Status::ResourceExausted();
      }

      // Note lack of else block due to continue.
      while (MyCondition()) {
        if (SomeEarlyBail()) {
          continue;
        }
        // Main handling of item
        ...
      }

      DoOtherThing();
      return OkStatus();

.. admonition:: **No**: Else after return needlessly creeps right
   :class: error

   .. code:: cpp

      if (Failure()) {
        DoTheThing();
        return Status::ResourceExausted();
      } else {
        while (MyCondition()) {
          if (SomeEarlyBail()) {
            continue;
          } else {
            // Main handling of item
            ...
          }
        }
        DoOtherThing();
        return OkStatus();
      }

Include guards
==============
The first non-comment line of every header file must be ``#pragma once``. Do
not use traditional macro include guards. The ``#pragma once`` should come
directly after the Pigweed copyright block, with no blank line, followed by a
blank, like this:

.. code-block:: cpp

  // Copyright 2021 The Pigweed Authors
  //
  // Licensed under the Apache License, Version 2.0 (the "License"); you may not
  // use this file except in compliance with the License. You may obtain a copy of
  // the License at
  //
  //     https://www.apache.org/licenses/LICENSE-2.0
  //
  // Unless required by applicable law or agreed to in writing, software
  // distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
  // WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
  // License for the specific language governing permissions and limitations under
  // the License.
  #pragma once

  // Header file-level comment goes here...

Memory allocation
=================
Dynamic memory allocation can be problematic. Heap allocations and deallocations
occupy valuable CPU cycles. Memory usage becomes nondeterministic, which can
result in a system crashing without a clear culprit.

To keep Pigweed portable, core Pigweed code is not permitted to dynamically
(heap) allocate memory, such as with ``malloc`` or ``new``. All memory should be
allocated with automatic (stack) or static (global) storage duration. Pigweed
must not use C++ libraries that use dynamic allocation.

Projects that use Pigweed are free to use dynamic allocation, provided they
have selected a target that enables the heap.

Naming
======
Entities shall be named according to the `Google style guide
<https://google.github.io/styleguide/cppguide.html>`_, with the following
additional requirements.

C++ code
--------
* All Pigweed C++ code must be in the ``pw`` namespace. Namespaces for modules
  should be nested under ``pw``. For example, ``pw::string::Format()``.
* Whenever possible, private code should be in a source (.cc) file and placed in
  anonymous namespace nested under ``pw``.
* If private code must be exposed in a header file, it must be in a namespace
  nested under ``pw``. The namespace may be named for its subsystem or use a
  name that designates it as private, such as ``internal``.
* Template arguments for non-type names (e.g. ``template <int kFooBar>``) should
  follow the constexpr and const variable Google naming convention, which means
  k prefixed camel case (e.g.  ``kCamelCase``). This matches the Google C++
  style for variable naming, however the wording in the official style guide
  isn't explicit for template arguments and could be interpreted to use
  ``foo_bar`` style naming.  For consistency with other variables whose value is
  always fixed for the duration of the program, the naming convention is
  ``kCamelCase``, and so that is the style we use in Pigweed.
* Trivial membor accessors should be named with ``snake_case()``. The Google
  C++ style allows either ``snake_case()`` or ``CapsCase()``, but Pigweed
  always uses ``snake_case()``.
* Abstract base classes should be named generically, with derived types named
  specifically. For example, ``Stream`` is an abstract base, and
  ``SocketStream`` and ``StdioStream`` are an implementations of that
  interface.  Any prefix or postfix indicating whether something is abstract or
  concrete is not permitted; for example, ``IStream`` or ``SocketStreamImpl``
  are both not permitted. These pre-/post-fixes add additional visual noise and
  are irrelevant to consumers of these interfaces.

C code
------
In general, C symbols should be prefixed with the module name. If the symbol is
not associated with a module, use just ``pw`` as the module name. Facade
backends may chose to prefix symbols with the facade's name to help reduce the
length of the prefix.

* Public names used by C code must be prefixed with the module name (e.g.
  ``pw_tokenizer_*``).
* If private code must be exposed in a header, private names used by C code must
  be prefixed with an underscore followed by the module name (e.g.
  ``_pw_assert_*``).
* Avoid writing C source (.c) files in Pigweed. Prefer to write C++ code with C
  linkage using ``extern "C"``. Within C source, private C functions and
  variables must be named with the ``_pw_my_module_*`` prefix and should be
  declared ``static`` whenever possible; for example,
  ``_pw_my_module_MyPrivateFunction``.
* The C prefix rules apply to

  * C functions (``int pw_foo_FunctionName(void);``),
  * variables used by C code (``int pw_foo_variable_name;``),
  * constant variables used by C code (``const int pw_foo_kConstantName;``),
  * structs used by C code (``typedef struct {} pw_foo_StructName;``), and
  * all of the above for ``extern "C"`` names in C++ code.

  The prefix does not apply to struct members, which use normal Google style.

Preprocessor macros
-------------------
* Public Pigweed macros must be prefixed with the module name (e.g.
  ``PW_MY_MODULE_*``).
* Private Pigweed macros must be prefixed with an underscore followed by the
  module name (e.g. ``_PW_MY_MODULE_*``). (This style may change, see
  `b/234886184 <https://issuetracker.google.com/issues/234886184>`_).

**Example**

.. code-block:: cpp

  namespace pw::my_module {
  namespace nested_namespace {

  // C++ names (types, variables, functions) must be in the pw namespace.
  // They are named according to the Google style guide.
  constexpr int kGlobalConstant = 123;

  // Prefer using functions over extern global variables.
  extern int global_variable;

  class Class {};

  void Function();

  extern "C" {

  // Public Pigweed code used from C must be prefixed with pw_.
  extern const int pw_my_module_kGlobalConstant;

  extern int pw_my_module_global_variable;

  void pw_my_module_Function(void);

  typedef struct {
    int member_variable;
  } pw_my_module_Struct;

  // Private Pigweed code used from C must be prefixed with _pw_.
  extern const int _pw_my_module_kPrivateGlobalConstant;

  extern int _pw_my_module_private_global_variable;

  void _pw_my_module_PrivateFunction(void);

  typedef struct {
    int member_variable;
  } _pw_my_module_PrivateStruct;

  }  // extern "C"

  // Public macros must be prefixed with PW_.
  #define PW_MY_MODULE_PUBLIC_MACRO(arg) arg

  // Private macros must be prefixed with _PW_.
  #define _PW_MY_MODULE_PRIVATE_MACRO(arg) arg

  }  // namespace nested_namespace
  }  // namespace pw::my_module

See :ref:`docs-pw-style-macros` for details about macro usage.

Namespace scope formatting
==========================
All non-indented blocks (namespaces, ``extern "C"`` blocks, and preprocessor
conditionals) must have a comment on their closing line with the
contents of the starting line.

All nested namespaces should be declared together with no blank lines between
them.

.. code-block:: cpp

  #include "some/header.h"

  namespace pw::nested {
  namespace {

  constexpr int kAnonConstantGoesHere = 0;

  }  // namespace

  namespace other {

  const char* SomeClass::yes = "no";

  bool ThisIsAFunction() {
  #if PW_CONFIG_IS_SET
    return true;
  #else
    return false;
  #endif  // PW_CONFIG_IS_SET
  }

  extern "C" {

  const int pw_kSomeConstant = 10;
  int pw_some_global_variable = 600;

  void pw_CFunction() { ... }

  }  // extern "C"

  }  // namespace
  }  // namespace pw::nested

Using directives for literals
=============================
`Using-directives
<https://en.cppreference.com/w/cpp/language/namespace#Using-directives>`_ (e.g.
``using namespace ...``) are permitted in implementation files only for the
purposes of importing literals such as ``std::chrono_literals`` or
``pw::bytes::unit_literals``. Namespaces that contain any symbols other than
literals are not permitted in a using-directive. This guidance also has no
impact on `using-declarations
<https://en.cppreference.com/w/cpp/language/namespace#Using-declarations>`_
(e.g. ``using foo::Bar;``).

Rationale: Literals improve code readability, making units clearer at the point
of definition.

.. code-block:: cpp

  using namespace std::chrono;                    // Not allowed
  using namespace std::literals::chrono_literals; // Allowed

  constexpr std::chrono::duration delay = 250ms;

Pointers and references
=======================
For pointer and reference types, place the asterisk or ampersand next to the
type.

.. code-block:: cpp

  int* const number = &that_thing;
  constexpr const char* kString = "theory!"

  bool FindTheOneRing(const Region& where_to_look) { ... }

Prefer storing references over storing pointers. Pointers are required when the
pointer can change its target or may be ``nullptr``. Otherwise, a reference or
const reference should be used.

.. _docs-pw-style-macros:

Preprocessor macros
===================
Macros should only be used when they significantly improve upon the C++ code
they replace. Macros should make code more readable, robust, and safe, or
provide features not possible with standard C++, such as stringification, line
number capturing, or conditional compilation. When possible, use C++ constructs
like constexpr variables in place of macros. Never use macros as constants,
except when a string literal is needed or the value must be used by C code.

When macros are needed, the macros should be accompanied with extensive tests
to ensure the macros are hard to use wrong.

Stand-alone statement macros
----------------------------
Macros that are standalone statements must require the caller to terminate the
macro invocation with a semicolon (see `Swalling the Semicolon
<https://gcc.gnu.org/onlinedocs/cpp/Swallowing-the-Semicolon.html>`_). For
example, the following does *not* conform to Pigweed's macro style:

.. code-block:: cpp

  // BAD! Definition has built-in semicolon.
  #define PW_LOG_IF_BAD(mj) \
    CallSomeFunction(mj);

  // BAD! Compiles without error; semicolon is missing.
  PW_LOG_IF_BAD("foo")

Here's how to do this instead:

.. code-block:: cpp

  // GOOD; requires semicolon to compile.
  #define PW_LOG_IF_BAD(mj) \
    CallSomeFunction(mj)

  // GOOD; fails to compile due to lacking semicolon.
  PW_LOG_IF_BAD("foo")

For macros in function scope that do not already require a semicolon, the
contents can be placed in a ``do { ... } while (0)`` loop.

.. code-block:: cpp

  #define PW_LOG_IF_BAD(mj)  \
    do {                     \
      if (mj.Bad()) {        \
        Log(#mj " is bad")   \
      }                      \
    } while (0)

Standalone macros at global scope that do not already require a semicolon can
add a ``static_assert`` declaration statement as their last line.

.. code-block:: cpp

  #define PW_NEAT_THING(thing)             \
    bool IsNeat_##thing() { return true; } \
    static_assert(true, "Macros must be terminated with a semicolon")

Private macros in public headers
--------------------------------
Private macros in public headers must be prefixed with ``_PW_``, even if they
are undefined after use; this prevents collisions with downstream users. For
example:

.. code-block:: cpp

  #define _PW_MY_SPECIAL_MACRO(op) ...
  ...
  // Code that uses _PW_MY_SPECIAL_MACRO()
  ...
  #undef _PW_MY_SPECIAL_MACRO

Macros in private implementation files (.cc)
--------------------------------------------
Macros within .cc files that should only be used within one file should be
undefined after their last use; for example:

.. code-block:: cpp

  #define DEFINE_OPERATOR(op) \
    T operator ## op(T x, T y) { return x op y; } \
    static_assert(true, "Macros must be terminated with a semicolon") \

  DEFINE_OPERATOR(+);
  DEFINE_OPERATOR(-);
  DEFINE_OPERATOR(/);
  DEFINE_OPERATOR(*);

  #undef DEFINE_OPERATOR

Preprocessor conditional statements
===================================
When using macros for conditional compilation, prefer to use ``#if`` over
``#ifdef``. This checks the value of the macro rather than whether it exists.

* ``#if`` handles undefined macros equivalently to ``#ifdef``. Undefined
  macros expand to 0 in preprocessor conditional statements.
* ``#if`` evaluates false for macros defined as 0, while ``#ifdef`` evaluates
  true.
* Macros defined using compiler flags have a default value of 1 in GCC and
  Clang, so they work equivalently for ``#if`` and ``#ifdef``.
* Macros defined to an empty statement cause compile-time errors in ``#if``
  statements, which avoids ambiguity about how the macro should be used.

All ``#endif`` statements should be commented with the expression from their
corresponding ``#if``. Do not indent within preprocessor conditional statements.

.. code-block:: cpp

  #if USE_64_BIT_WORD
  using Word = uint64_t;
  #else
  using Word = uint32_t;
  #endif  // USE_64_BIT_WORD

Unsigned integers
=================
Unsigned integers are permitted in Pigweed. Aim for consistency with existing
code and the C++ Standard Library. Be very careful mixing signed and unsigned
integers.

Features not in the C++ standard
================================
Avoid features not available in standard C++. This includes compiler extensions
and features from other standards like POSIX.

For example, use ``ptrdiff_t`` instead of POSIX's ``ssize_t``, unless
interacting with a POSIX API in intentionally non-portable code. Never use
POSIX functions with suitable standard or Pigweed alternatives, such as
``strnlen`` (use ``pw::string::NullTerminatedLength`` instead).

.. _owners-style:

--------------------
Code Owners (OWNERS)
--------------------
If you use Gerrit for source code hosting and have the
`code-owners <https://android-review.googlesource.com/plugins/code-owners/Documentation/backend-find-owners.html>`__
plug-in enabled Pigweed can help you enforce consistent styling on those files
and also detects some errors.

The styling follows these rules.

#. Content is grouped by type of line (Access grant, include, etc).
#. Each grouping is sorted alphabetically.
#. Groups are placed the following order with a blank line separating each
   grouping.

    * "set noparent" line
    * "include" lines
    * "file:" lines
    * user grants (some examples: "*", "foo@example.com")
    * "per-file:" lines

This plugin will, by default, act upon any file named "OWNERS".

.. _python-style:

------------
Python style
------------
Pigweed uses the standard Python style: PEP8, which is available on the web at
https://www.python.org/dev/peps/pep-0008/. All Pigweed Python code should pass
``pw format``, which invokes ``black`` with a couple options.

Python versions
===============
Pigweed officially supports :ref:`a few Python versions
<docs-concepts-python-version>`. Upstream Pigweed code must support those Python
versions. The only exception is :ref:`module-pw_env_setup`, which must also
support Python 2 and 3.6.

---------------
Build files: GN
---------------
Each Pigweed source module requires a GN build file named BUILD.gn. This
encapsulates the build targets and specifies their sources and dependencies.
GN build files use a format similar to `Bazel's BUILD files
<https://docs.bazel.build/versions/main/build-ref.html>`_
(see the `Bazel style guide
<https://docs.bazel.build/versions/main/skylark/build-style.html>`_).

C/C++ build targets include a list of fields. The primary fields are:

* ``<public>`` -- public header files
* ``<sources>`` -- source files and private header files
* ``<public_configs>`` -- public build configuration
* ``<configs>`` -- private build configuration
* ``<public_deps>`` -- public dependencies
* ``<deps>`` -- private dependencies

Assets within each field must be listed in alphabetical order.

.. code-block:: cpp

  # Here is a brief example of a GN build file.

  import("$dir_pw_unit_test/test.gni")

  config("public_include_path") {
    include_dirs = [ "public" ]
    visibility = [":*"]
  }

  pw_source_set("pw_sample_module") {
    public = [ "public/pw_sample_module/sample_module.h" ]
    sources = [
      "public/pw_sample_module/internal/secret_header.h",
      "sample_module.cc",
      "used_by_sample_module.cc",
    ]
    public_configs = [ ":public_include_path" ]
    public_deps = [ dir_pw_status ]
    deps = [ dir_pw_varint ]
  }

  pw_test_group("tests") {
    tests = [ ":sample_module_test" ]
  }

  pw_test("sample_module_test") {
    sources = [ "sample_module_test.cc" ]
    deps = [ ":sample_module" ]
  }

  pw_doc_group("docs") {
    sources = [ "docs.rst" ]
  }

------------------
Build files: Bazel
------------------
Build files for the Bazel build system must be named ``BUILD.bazel``. Bazel can
interpret files named just ``BUILD``, but Pigweed uses ``BUILD.bazel`` to avoid
ambiguity with other build systems or tooling.

Pigweed's Bazel files follow the `Bazel style guide
<https://docs.bazel.build/versions/main/skylark/build-style.html>`_.

.. _documentation-style:

-------------
Documentation
-------------
.. note::

   Pigweed's documentation style guide came after much of the documentation was
   written, so Pigweed's docs don't yet 100% conform to this style guide. When
   updating docs, please update them to match the style guide.

Pigweed documentation is written using the `reStructuredText
<https://docutils.sourceforge.io/rst.html>`_ markup language and processed by
`Sphinx`_. We use the `Furo theme <https://github.com/pradyunsg/furo>`_ along
with the `sphinx-design <https://sphinx-design.readthedocs.io/en/furo-theme/>`_
extension.

Syntax Reference Links
======================
.. admonition:: See also
   :class: seealso

   - `reStructuredText Primer`_

   - `reStructuredText Directives <https://docutils.sourceforge.io/docs/ref/rst/directives.html>`_

   - `Furo Reference <https://pradyunsg.me/furo/reference/>`_

   - `Sphinx-design Reference <https://sphinx-design.readthedocs.io/en/furo-theme/>`_

ReST is flexible, supporting formatting the same logical document in a few ways
(for example headings, blank lines). Pigweed has the following restrictions to
make our documentation consistent.

Headings
========
Use headings according to the following hierarchy, with the shown characters
for the ReST heading syntax.

.. code:: rst

   ==================================
   Document Title: Two Bars of Equals
   ==================================
   Document titles use equals ("====="), above and below. Capitalize the words
   in the title, except for 'a', 'of', and 'the'.

   ---------------------------
   Major Sections Within a Doc
   ---------------------------
   Major sections use hyphens ("----"), above and below. Capitalize the words in
   the title, except for 'a', 'of', and 'the'.

   Heading 1 - For Sections Within a Doc
   =====================================
   These should be title cased. Use a single equals bar ("====").

   Heading 2 - for subsections
   ---------------------------
   Subsections use hyphens ("----"). In many cases, these headings may be
   sentence-like. In those cases, only the first letter should be capitalized.
   For example, FAQ subsections would have a title with "Why does the X do the
   Y?"; note the sentence capitalization (but not title capitalization).

   Heading 3 - for subsubsections
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   Use the caret symbol ("^^^^") for subsubsections.

   Note: Generally don't go beyond heading 3.

   Heading 4 - for subsubsubsections
   .................................
   Don't use this heading level, but if you must, use period characters
   ("....") for the heading.

Do not put blank lines after headings.
--------------------------------------
.. admonition:: **Yes**: No blank after heading
   :class: checkmark

   .. code:: rst

      Here is a heading
      -----------------
      Note that there is no blank line after the heading separator!

.. admonition:: **No**: Unnecessary blank line
   :class: error

   .. code:: rst

      Here is a heading
      -----------------

      There is a totally unnecessary blank line above this one. Don't do this.

Do not put multiple blank lines before a heading.
-------------------------------------------------
.. admonition:: **Yes**: Just one blank after section content before the next heading
   :class: checkmark

   .. code:: rst

      There is some text here in the section before the next. It's just here to
      illustrate the spacing standard. Note that there is just one blank line
      after this paragraph.

      Just one blank!
      ---------------
      There is just one blank line before the heading.

.. admonition:: **No**: Extra blank lines
   :class: error

   .. code:: rst

      There is some text here in the section before the next. It's just here to
      illustrate the spacing standard. Note that there are too many blank lines
      after this paragraph; there should be just one.



      Too many blanks
      ---------------
      There are too many blanks before the heading for this section.

Directives
==========
Indent directives 3 spaces; and put a blank line between the directive and the
content. This aligns the directive content with the directive name.

.. admonition:: **Yes**: Three space indent for directives; and nested
   :class: checkmark

   .. code:: none

      Here is a paragraph that has some content. After this content is a
      directive.

      .. my_directive::

         Note that this line's start aligns with the "m" above. The 3-space
         alignment accounts for the ".. " prefix for directives, to vertically
         align the directive name with the content.

         This indentation must continue for nested directives.

         .. nested_directive::

            Here is some nested directive content.

.. admonition:: **No**: One space, two spaces, four spaces, or other indents
   for directives
   :class: error

   .. code:: none

      Here is a paragraph with some content.

      .. my_directive::

        The indentation here is incorrect! It's one space short; doesn't align
        with the directive name above.

        .. nested_directive::

            This isn't indented correctly either; it's too much (4 spaces).

.. admonition:: **No**: Missing blank between directive and content.
   :class: error

   .. code:: none

      Here is a paragraph with some content.

      .. my_directive::
         Note the lack of blank line above here.

Tables
======
Consider using ``.. list-table::`` syntax, which is more maintainable and
easier to edit for complex tables (`details
<https://docutils.sourceforge.io/docs/ref/rst/directives.html#list-table>`_).

Code Snippets
=============
Use code blocks from actual source code files wherever possible. This helps keep
documentation fresh and removes duplicate code examples. There are a few ways to
do this with Sphinx.

The `literalinclude`_ directive creates a code blocks from source files. Entire
files can be included or a just a subsection. The best way to do this is with
the ``:start-after:`` and ``:end-before:`` options.

Example:

.. card::

   Documentation Source (``.rst`` file)
   ^^^

   .. code-block:: rst

      .. literalinclude:: run_doxygen.py
         :start-after: [doxygen-environment-variables]
         :end-before: [doxygen-environment-variables]

.. card::

   Source File
   ^^^

   .. code-block::

      # DOCSTAG: [doxygen-environment-variables]
      env = os.environ.copy()
      env['PW_DOXYGEN_OUTPUT_DIRECTORY'] = str(output_dir.resolve())
      env['PW_DOXYGEN_INPUT'] = ' '.join(pw_module_list)
      env['PW_DOXYGEN_PROJECT_NAME'] = 'Pigweed'
      # DOCSTAG: [doxygen-environment-variables]

.. card::

   Rendered Output
   ^^^

   .. code-block::

      env = os.environ.copy()
      env['PW_DOXYGEN_OUTPUT_DIRECTORY'] = str(output_dir.resolve())
      env['PW_DOXYGEN_INPUT'] = ' '.join(pw_module_list)
      env['PW_DOXYGEN_PROJECT_NAME'] = 'Pigweed'

Generating API documentation from source
========================================
Whenever possible, document APIs in the source code and use Sphinx to generate
documentation for them. This keeps the documentation in sync with the code and
reduces duplication.

Python
------
Include Python API documentation from docstrings with `autodoc directives`_.
Example:

.. code-block:: rst

   .. automodule:: pw_cli.log
      :members:

   .. automodule:: pw_console.embed
      :members: PwConsoleEmbed
      :undoc-members:
      :show-inheritance:

   .. autoclass:: pw_console.log_store.LogStore
      :members: __init__
      :undoc-members:
      :show-inheritance:

Include argparse command line help with the `argparse
<https://sphinx-argparse.readthedocs.io/en/latest/usage.html>`_
directive. Example:

.. code-block:: rst

   .. argparse::
      :module: pw_watch.watch
      :func: get_parser
      :prog: pw watch
      :nodefaultconst:
      :nodescription:
      :noepilog:


Doxygen
-------
Doxygen comments in C, C++, and Java are surfaced in Sphinx using `Breathe
<https://breathe.readthedocs.io/en/latest/index.html>`_.

.. note::

   Sources with doxygen comment blocks must be added to the
   ``_doxygen_input_files`` list in ``//docs/BUILD.gn`` to be processed.

Breathe provides various `directives
<https://breathe.readthedocs.io/en/latest/directives.html>`_ for bringing
Doxygen comments into Sphinx. These include the following:

- `doxygenfile
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygenfile>`_ --
  Documents a source file. May limit to specific types of symbols with
  ``:sections:``.

  .. code-block:: rst

     .. doxygenfile:: pw_rpc/internal/config.h
        :sections: define, func

- `doxygenclass
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygenclass>`_ --
  Documents a class and optionally its members.

  .. code-block:: rst

     .. doxygenclass:: pw::sync::BinarySemaphore
        :members:

- `doxygentypedef
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygentypedef>`_ --
  Documents an alias (``typedef`` or ``using`` statement).

  .. code-block:: rst

     .. doxygentypedef:: pw::Function

- `doxygenfunction
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygenfunction>`_ --
  Documents a source file. Can be filtered to limit to specific types of
  symbols.

  .. code-block:: rst

     .. doxygenfunction:: pw::tokenizer::EncodeArgs

- `doxygendefine
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygendefine>`_ --
  Documents a preprocessor macro.

  .. code-block:: rst

     .. doxygendefine:: PW_TOKENIZE_STRING

.. admonition:: See also

  `All Breathe directives for use in RST files <https://breathe.readthedocs.io/en/latest/directives.html>`_

Example Doxygen Comment Block
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Start a Doxygen comment block using ``///`` (three forward slashes).

.. code-block:: cpp

   /// This is the documentation comment for the `PW_LOCK_RETURNED()` macro. It
   /// describes how to use the macro.
   ///
   /// Doxygen comments can refer to other symbols using Sphinx cross
   /// references. For example, @cpp_class{pw::InlineBasicString}, which is
   /// shorthand for @crossref{cpp,class,pw::InlineBasicString}, links to a C++
   /// class. @crossref{py,func,pw_tokenizer.proto.detokenize_fields} links to a
   /// Python function.
   ///
   /// @param[out] dest The memory area to copy to.
   /// @param[in]  src  The memory area to copy from.
   /// @param[in]  n    The number of bytes to copy
   ///
   /// @retval OK KVS successfully initialized.
   /// @retval DATA_LOSS KVS initialized and is usable, but contains corrupt data.
   /// @retval UNKNOWN Unknown error. KVS is not initialized.
   ///
   /// @rst
   /// The ``@rst`` and ``@endrst`` commands form a block block of
   /// reStructuredText that is rendered in Sphinx.
   ///
   /// .. warning::
   ///    this is a warning admonition
   ///
   /// .. code-block:: cpp
   ///
   ///    void release(ptrdiff_t update = 1);
   /// @endrst
   ///
   /// Example code block using Doxygen markup below. To override the language
   /// use `@code{.cpp}`
   ///
   /// @code
   ///   class Foo {
   ///    public:
   ///     Mutex* mu() PW_LOCK_RETURNED(mu) { return &mu; }
   ///
   ///    private:
   ///     Mutex mu;
   ///   };
   /// @endcode
   ///
   /// @b The first word in this sentence is bold (The).
   ///
   #define PW_LOCK_RETURNED(x) __attribute__((lock_returned(x)))

Doxygen Syntax
^^^^^^^^^^^^^^
Pigweed prefers to use RST wherever possible, but there are a few Doxygen
syntatic elements that may be needed.

Common Doxygen commands for use within a comment block:

- ``@rst`` To start a reStructuredText block. This is a custom alias for
  ``\verbatim embed:rst:leading-asterisk``.
- `@code <https://www.doxygen.nl/manual/commands.html#cmdcode>`_ Start a code
  block.
- `@param <https://www.doxygen.nl/manual/commands.html#cmdparam>`_ Document
  parameters, this may be repeated.
- `@pre <https://www.doxygen.nl/manual/commands.html#cmdpre>`_ Starts a
  paragraph where the precondition of an entity can be described.
- `@post <https://www.doxygen.nl/manual/commands.html#cmdpost>`_ Starts a
  paragraph where the postcondition of an entity can be described.
- `@return <https://www.doxygen.nl/manual/commands.html#cmdreturn>`_ Single
  paragraph to describe return value(s).
- `@retval <https://www.doxygen.nl/manual/commands.html#cmdretval>`_ Document
  return values by name. This is rendered as a definition list.
- `@note <https://www.doxygen.nl/manual/commands.html#cmdnote>`_ Add a note
  admonition to the end of documentation.
- `@b <https://www.doxygen.nl/manual/commands.html#cmdb>`_ To bold one word.

Doxygen provides `structural commands
<https://doxygen.nl/manual/docblocks.html#structuralcommands>`_ that associate a
comment block with a particular symbol. Example of these include ``@class``,
``@struct``, ``@def``, ``@fn``, and ``@file``. Do not use these unless
necessary, since they are redundant with the declarations themselves.

One case where structural commands are necessary is when a single comment block
describes multiple symbols. To group multiple symbols into a single comment
block, include a structural commands for each symbol on its own line. For
example, the following comment documents two macros:

.. code-block:: cpp

   /// @def PW_ASSERT_EXCLUSIVE_LOCK
   /// @def PW_ASSERT_SHARED_LOCK
   ///
   /// Documents functions that dynamically check to see if a lock is held, and
   /// fail if it is not held.

.. seealso:: `All Doxygen commands <https://www.doxygen.nl/manual/commands.html>`_

Cross-references
^^^^^^^^^^^^^^^^
Pigweed provides Doxygen aliases for creating Sphinx cross references within
Doxygen comments. These should be used when referring to other symbols, such as
functions, classes, or macros.

.. inclusive-language: disable

The basic alias is ``@crossref``, which supports any `Sphinx domain
<https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html>`_.
For readability, aliases for commonnly used types are provided.

.. inclusive-language: enable

- ``@crossref{domain,type,identifier}`` Inserts a cross reference of any type in
  any Sphinx domain. For example, ``@crossref{c,func,foo}`` is equivalent to
  ``:c:func:`foo``` and links to the documentation for the C function ``foo``,
  if it exists.
- ``@c_macro{identifier}`` Equivalent to ``:c:macro:`identifier```.
- ``@cpp_func{identifier}`` Equivalent to ``:cpp:func:`identifier```.
- ``@cpp_class{identifier}`` Equivalent to ``:cpp:class:`identifier```.
- ``@cpp_type{identifier}`` Equivalent to ``:cpp:type:`identifier```.

.. note::

   Use the `@` aliases described above for all cross references. Doxygen
   provides other methods for cross references, but Sphinx cross references
   offer several advantages:

   - Sphinx cross references work for all identifiers known to Sphinx, including
     those documented with e.g. ``.. cpp:class::`` or extracted from Python.
     Doxygen references can only refer to identifiers known to Doxygen.
   - Sphinx cross references always use consistent formatting. Doxygen cross
     references sometimes render as plain text instead of code-style monospace,
     or include ``()`` in macros that shouldn't have them.
   - Sphinx cross references can refer to symbols that have not yet been
     documented. They will be formatted correctly and become links once the
     symbols are documented.
   - Using Sphinx cross references in Doxygen comments makes cross reference
     syntax more consistent within Doxygen comments and between RST and
     Doxygen.

Create cross reference links elsewhere in the document to symbols documented
with Doxygen using standard Sphinx cross references, such as ``:cpp:class:`` and
``:cpp:func:``.

.. code-block:: rst

   - :cpp:class:`pw::sync::BinarySemaphore::BinarySemaphore`
   - :cpp:func:`pw::sync::BinarySemaphore::try_acquire`

.. seealso::
   - `C++ cross reference link syntax`_
   - `C cross reference link syntax`_
   - `Python cross reference link syntax`_

.. _Sphinx: https://www.sphinx-doc.org/

.. inclusive-language: disable

.. _reStructuredText Primer: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html
.. _literalinclude: https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-literalinclude
.. _C++ cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing
.. _C cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-c-constructs
.. _Python cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-python-objects
.. _autodoc directives: https://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html#directives

.. inclusive-language: enable

Status codes in Doxygen comments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Use the following syntax when referring to ``pw_status`` codes in Doxygen
comments:

.. code-block::

   @pw_status{YOUR_STATUS_CODE_HERE}

Replace ``YOUR_STATUS_CODE_HERE`` with a valid ``pw_status`` code.

This syntax ensures that Doxygen links back to the status code's
reference documentation in :ref:`module-pw_status`.

.. note::

   The guidance in this section only applies to Doxygen comments in C++ header
   files.

Customize the depth of a page's table of contents
=================================================
Put ``:tocdepth: X`` on the first line of the page, where ``X`` equals how many
levels of section heading you want to show in the page's table of contents. See
``//docs/changelog.rst`` for an example.

Changelog
=========

This section explains how we update the changelog.

#. On the Friday before Pigweed Live, use
   `changelog <https://kaycebasques.github.io/changelog/>`_ to generate a first
   draft of the changelog.

#. Copy-paste the reStructuredText output from the changelog tool to the top
   of ``//docs/changelog.rst``.

#. Delete these lines from the previous update in ``changelog.rst``
   (which is no longer the latest update):

   * ``.. _docs-changelog-latest:``
   * ``.. changelog_highlights_start``
   * ``.. changelog_highlights_end``

#. Polish up the auto-generated first draft into something more readable:

   * Don't change the section headings. The text in each section heading
     should map to one of the categories that we allow in our commit
     messages, such as ``bazel``, ``docs``, ``pw_base64``, and so on.
   * Add a 1-paragraph summary to each section.
   * Focus on features, important bug fixes, and breaking changes. Delete
     internal commits that Pigweed customers won't care about.

#. Push your change up to Gerrit and kick off a dry run. After a few minutes
   the docs will get staged.

#. Copy the rendered content from the staging site into the Pigweed Live
   Google Doc.

#. Make sure to land the changelog updates the same week as Pigweed Live.

There is no need to update ``//docs/index.rst``. The ``What's new in Pigweed``
content on the homepage is pulled from the changelog (that's what the
``docs-changelog-latest``, ``changelog_highlights_start``, and
``changelog_highlights_end`` labels are for).

Why "changelog" and not "release notes"?
----------------------------------------
Because Pigweed doesn't have releases.

Why organize by module / category?
----------------------------------
Why is the changelog organized by category / module? Why not the usual
3 top-level sections: features, fixes, breaking changes?

* Because some Pigweed customers only use a few modules. Organizing by module
  helps them filter out all the changes that aren't relevant to them faster.
* If we keep the changelog section heading text fairly structured, we may
  be able to present the changelog in other interesting ways. For example,
  it should be possible to collect every ``pw_base64`` section in the changelog
  and then provide a changelog for only ``pw_base64`` over in the ``pw_base64``
  docs.
* The changelog tool is easily able to organize by module / category due to
  how we annotate our commits. We will not be able to publish changelog updates
  every 2 weeks if there is too much manual work involved.

Site nav scrolling
==================
The site nav was customized (`change #162410`_) to scroll on initial page load
so that the current page is visible in the site nav. The scrolling logic is
handled in ``//docs/_static/js/pigweed.js``.

.. _change #162410: https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162410

.. _commit-style:

--------------
Commit message
--------------
Pigweed commit message bodies and summaries are limited to 72 characters wide
to improve readability. Commit summaries should also be prefixed with the name
of the module that the commit is affecting. :ref:`Examples
<docs-contributing-commit-message-examples>` of well and ill-formed commit
messages are provided below.

Consider the following when writing a commit message:

#. **Documentation and comments are better** - Consider whether the commit
   message contents would be better expressed in the documentation or code
   comments. Docs and code comments are durable and readable later; commit
   messages are rarely read after the change lands.
#. **Include why the change is made, not just what the change is** - It is
   important to include a "why" component in most commits. Sometimes, why is
   evident - for example, reducing memory usage, or optimizing. But it is often
   not. Err on the side of over-explaining why, not under-explaining why.

.. _docs-contributing-commit-message-examples:

Pigweed commit messages should conform to the following style:

.. admonition:: **Yes**:
   :class: checkmark

   .. code:: none

      pw_some_module: Short capitalized description

      Details about the change here. Include a summary of the what, and a clear
      description of why the change is needed for future maintainers.

      Consider what parts of the commit message are better suited for
      documentation.

.. admonition:: **Yes**: Small number of modules affected; use {} syntax.
   :class: checkmark

   .. code:: none

      pw_{foo, bar, baz}: Change something in a few places

      When changes cross a few modules, include them with the syntax shown
      above.

.. admonition:: **Yes**: Targets are effectively modules, even though they're
   nested, so they get a ``/`` character.
   :class: checkmark

   .. code:: none

      targets/xyz123: Tweak support for XYZ's PQR

.. admonition:: **Yes**: Uses imperative style for subject and text.
   :class: checkmark

   .. code:: none

      pw_something: Add foo and bar functions

      This commit correctly uses imperative present-tense style.

.. admonition:: **No**: Uses non-imperative style for subject and text.
   :class: error

   .. code:: none

      pw_something: Adds more things

      Use present tense imperative style for subjects and commit. The above
      subject has a plural "Adds" which is incorrect; should be "Add".

.. admonition:: **Yes**: Use bulleted lists when multiple changes are in a
   single CL. Prefer smaller CLs, but larger CLs are a practical reality.
   :class: checkmark

   .. code:: none

      pw_complicated_module: Pre-work for refactor

      Prepare for a bigger refactor by reworking some arguments before the larger
      change. This change must land in downstream projects before the refactor to
      enable a smooth transition to the new API.

      - Add arguments to MyImportantClass::MyFunction
      - Update MyImportantClass to handle precondition Y
      - Add stub functions to be used during the transition

.. admonition:: **No**: Run on paragraph instead of bulleted list
   :class: error

   .. code:: none

      pw_foo: Many things in a giant BWOT

      This CL does A, B, and C. The commit message is a Big Wall Of Text
      (BWOT), which we try to discourage in Pigweed. Also changes X and Y,
      because Z and Q. Furthermore, in some cases, adds a new Foo (with Bar,
      because we want to). Also refactors qux and quz.

.. admonition:: **No**: Doesn't capitalize the subject
   :class: error

   .. code:: none

      pw_foo: do a thing

      Above subject is incorrect, since it is a sentence style subject.

.. admonition:: **Yes**: Doesn't capitalize the subject when subject's first
   word is a lowercase identifier.
   :class: checkmark

   .. code:: none

      pw_foo: std::unique_lock cleanup

      This commit message demonstrates the subject when the subject has an
      identifier for the first word. In that case, follow the identifier casing
      instead of capitalizing.

   However, imperative style subjects often have the identifier elsewhere in
   the subject; for example:

   .. code:: none

     pw_foo: Improve use of std::unique_lock

.. admonition:: **No**: Uses a non-standard ``[]`` to indicate module:
   :class: error

   .. code:: none

      [pw_foo]: Do a thing

.. admonition:: **No**: Has a period at the end of the subject
   :class: error

   .. code:: none

      pw_bar: Do something great.

.. admonition:: **No**: Puts extra stuff after the module which isn't a module.
   :class: error

   .. code:: none

      pw_bar/byte_builder: Add more stuff to builder

Footer
======
We support a number of `git footers`_ in the commit message, such as ``Bug:
123`` in the message below:

.. code:: none

   pw_something: Add foo and bar functions

   Bug: 123

You are encouraged to use the following footers when appropriate:

* ``Bug``: Associates this commit with a bug (issue in our `bug tracker`_). The
  bug will be automatically updated when the change is submitted. When a change
  is relevant to more than one bug, include multiple ``Bug`` lines, like so:

  .. code:: none

      pw_something: Add foo and bar functions

      Bug: 123
      Bug: 456

* ``Fixed`` or ``Fixes``: Like ``Bug``, but automatically closes the bug when
  submitted.

  .. code:: none

      pw_something: Fix incorrect use of foo

      Fixes: 123

In addition, we support all of the `Chromium CQ footers`_, but those are
relatively rarely useful.

.. _bug tracker: https://bugs.chromium.org/p/pigweed/issues/list
.. _Chromium CQ footers: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/infra/cq.md#options
.. _git footers: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/git-footers.html

Copy-to-clipboard feature on code blocks
========================================

.. _sphinx-copybutton: https://sphinx-copybutton.readthedocs.io/en/latest/
.. _Remove copybuttons using a CSS selector: https://sphinx-copybutton.readthedocs.io/en/latest/use.html#remove-copybuttons-using-a-css-selector

The copy-to-clipboard feature on code blocks is powered by `sphinx-copybutton`_.

``sphinx-copybutton`` recognizes ``$`` as an input prompt and automatically
removes it.

There is a workflow for manually removing the copy-to-clipboard button for a
particular code block but it has not been implemented yet. See
`Remove copybuttons using a CSS selector`_.

Grouping related content with tabs
==================================
Use the ``tabs`` directive to group related content together. This feature is
powered by `sphinx-tabs <https://sphinx-tabs.readthedocs.io>`_.

Tabs for code-only content
--------------------------
Use the ``tabs`` and ``code-tab`` directives together. Example:

.. code:: none

   .. tabs::

      .. code-tab:: c++

         // C++ code...

      .. code-tab:: py

         # Python code...

Rendered output:

.. tabs::

   .. code-tab:: c++

      // C++ code...

   .. code-tab:: py

      # Python code...

Tabs for all other content
--------------------------
Use the ``tabs`` and ``group-tab`` directives together. Example:

.. code:: none

   .. tabs::

      .. group-tab:: Linux

         Linux instructions...

      .. group-tab:: Windows

         Windows instructions...

Rendered output:

.. tabs::

   .. group-tab:: Linux

      Linux instructions...

   .. group-tab:: Windows

      Windows instructions...

Tab synchronization
-------------------
Tabs are synchronized based on ``group-tab`` and ``code-tab`` values. Example:

.. code:: none

   .. tabs::

      .. code-tab:: c++

         // C++ code...

      .. code-tab:: py

         # Python code...

   .. tabs::

      .. code-tab:: c++

         // More C++ code...

      .. code-tab:: py

         # More Python code...

   .. tabs::

      .. group-tab:: Linux

         Linux instructions...

      .. group-tab:: Windows

         Windows instructions...

   .. tabs::

      .. group-tab:: Linux

         More Linux instructions...

      .. group-tab:: Windows

         More Windows instructions...

Rendered output:

.. tabs::

   .. code-tab:: c++

      // C++ code...

   .. code-tab:: py

      # Python code...

.. tabs::

   .. code-tab:: c++

      // More C++ code...

   .. code-tab:: py

      # More Python code...

.. tabs::

   .. group-tab:: Linux

      Linux instructions...

   .. group-tab:: Windows

      Windows instructions...

.. tabs::

   .. group-tab:: Linux

      More Linux instructions...

   .. group-tab:: Windows

      More Windows instructions...


