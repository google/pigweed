.. _docs-pw-style:

===========
Style Guide
===========
.. tip::
  Pigweed runs ``pw format`` as part of ``pw presubmit`` to perform some code
  formatting checks. To speed up the review process, consider adding ``pw
  presubmit`` as a git push hook using the following command:
  ``pw presubmit --install``

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
Pigweed primarily uses the C++17 standard. A few modules maintain support for
C++14, however (e.g. :ref:`module-pw_kvs` and its dependencies).

All Pigweed C++ code must compile with ``-std=C++17`` in Clang and GCC. C++20
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
   * ``<span>``
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
   * ``<functional>`` -- do **not** use ``std::function``
   * ``<new>`` -- for placement new
   * ``<numeric>`` -- be wary of code size with multiple template instantiations

.. admonition:: Never use any of these headers:
   :class: error

   * Dynamic containers (``<list>``, ``<map>``, ``<set>``, ``<vector>``, etc.)
   * Streams (``<iostream>``, ``<ostream>``, ``<fstream>``, etc.)
   * ``<exception>``
   * ``<future>``, ``<mutex>``, ``<thread>``
   * ``<memory>``
   * ``<regex>``
   * ``<scoped_allocator>``
   * ``<sstream>``
   * ``<stdexcept>``
   * ``<string>``
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
All loops and conditional statements must use braces.

The syntax ``while (true)`` is preferred over ``for (;;)`` for infinite loops.

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
  * constant variables used by C code (``int pw_foo_kConstantName;``),
  * structs used by C code (``typedef struct {} pw_foo_StructName;``), and
  * all of the above for ``extern "C"`` names in C++ code.

  The prefix does not apply to struct members, which use normal Google style.

Preprocessor macros
-------------------
* Public Pigweed macros must be prefixed with the module name (e.g.
  ``PW_MY_MODULE_*``).
* Private Pigweed macros must be prefixed with an underscore followed by the
  module name (e.g. ``_PW_MY_MODULE_*``).

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
macro invocation with a semicolon. For example, the following does *not* conform
to Pigweed's macro style:

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
add a ``static_assert`` or throwaway struct declaration statement as their
last line.

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
Macros within .cc files that should only used within one file should be
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

------------
Python style
------------
Pigweed uses the standard Python style: PEP8, which is available on the web at
https://www.python.org/dev/peps/pep-0008/. All Pigweed Python code should pass
``yapf`` when configured for PEP8 style.

Python 3
========
Pigweed uses Python 3. Some modules may offer limited support for Python 2, but
Python 3.6 or newer is required for most Pigweed code.

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
   in the title, except for 'of' and 'the'.

   ---------------------------
   Major Sections Within a Doc
   ---------------------------
   Major sections use hypens ("----"), above and below. Capitalize the words in
   the title, except for 'of' and 'the'.

   Heading 1 - For Sections Within a Doc
   =====================================
   These should be title cased. Use a single equals bar ("====").

   Heading 2 - for subsections
   ---------------------------
   Subsections use hypens ("----"). In many cases, these headings may be
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

.. _Sphinx: https://www.sphinx-doc.org/

.. inclusive-language: disable

.. _reStructuredText Primer: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html

.. inclusive-language: enable
