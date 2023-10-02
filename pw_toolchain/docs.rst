.. _module-pw_toolchain:

============
pw_toolchain
============
GN toolchains function both as a set of tools for compilation and as a workspace
for evaluating build files. The same compilations and actions can be executed by
different toolchains. Each toolchain maintains its own set of build args, and
build steps from all toolchains can be executed in parallel.

----------
Toolchains
----------
``pw_toolchain`` module provides GN toolchains that may be used to build
Pigweed. Various GCC and Clang toolchains for multiple platforms are provided.
Toolchains names typically include the compiler (``clang`` or ``gcc`` and
optimization level (``debug``, ``size_optimized``, ``speed_optimized``).

--------------------
Non-C/C++ toolchains
--------------------
``pw_toolchain/non_c_toolchain.gni`` provides the ``pw_non_c_toolchain``
template. This template creates toolchains that cannot compile C/C++ source
code. These toolchains may only be used to execute GN actions or declare groups
of targets in other toolchains. Attempting to compile C/C++ code with either of
these toolchains results in errors.

Non-C/C++ toolchains can be used to consolidate actions that should only occur
once in a multi-toolchain build. Build targets from all toolchains can refer to
these actions in a non-C/C++ toolchain so they only execute once instead of once
per toolchain.

For example, Pigweed runs protobuf compilation and Python package actions like
installation and Pylint in toolchains created with ``pw_non_c_toolchain``. This
allows all toolchains to cleanly share the same protobuf and Python declarations
without any duplicated work.

-------------------------------
Testing other compiler versions
-------------------------------
The clang-based toolchain provided by Pigweed can be substituted with another
version by modifying the ``pw_toolchain_CLANG_PREFIX`` GN build argument to
point to the directory that contains the desired clang, clang++, and llvm-ar
binaries. This should only be used for debugging purposes. Pigweed does not
officially support any compilers other than those provided by Pigweed.

------------------------------
Running static analysis checks
------------------------------
``clang-tidy`` can be run as a compiler replacement, to analyze all sources
built for a target. ``pw_toolchain/static_analysis_toolchain.gni`` provides
the ``pw_static_analysis_toolchain`` template. This template creates toolchains
that execute ``clang-tidy`` for C/C++ sources, and mock implementations of
the ``link``, ``alink`` and ``solink`` tools.

In addition to the standard toolchain requirements (`cc`, `cxx`, etc..), the
``pw_static_analysis_toolchain`` template requires a scope ``static_analysis``
to be defined on the invoker.

.. code-block::

   static_analysis = {
    # Configure whether static_analysis should be enabled for invoker toolchain.
    # This is must be set true if using pw_static_analysis_toolchain.
    enabled = true
    # Optionally override clang-tidy binary to use by setting to proper path.
    clang_tidy_path = ""
    # Optionally specify additional command(s) to run as part of cc tool.
    cc_post = ""
    # Optionally specify additional command(s) to run as part of cxx tool.
    cxx_post = ""
   }

The ``generate_toolchain`` supports the above mentioned ``static_analysis``
scope, which if specified must at the very least define the bool ``enabled``
within the scope. If the ``static_analysis`` scope is provided and
``static_analysis.enabled = true``, the derived toolchain
``${target_name}.static_analysis`` will be generated using
``pw_generate_static_analysis_toolchain`` and the toolchain options.

An example on the utility of the ``static_analysis`` scope args is shown in the
snippet below where we enable clang-tidy caching and add ``//.clang-tidy`` as a
dependency to the generated ``.d`` files for the
``pw_static_analysis_toolchain``.

.. code-block::

   static_analysis = {
    clang_tidy_path = "//third_party/ctcache/clang-tidy"
    _clang_tidy_cfg_path = rebase_path("//.clang-tidy", root_build_dir)
    cc_post = "echo '-: $_clang_tidy_cfg_path' >> {{output}}.d"
    cxx_post = "echo '-: $_clang_tidy_cfg_path' >> {{output}}.d"
   }

Excluding files from checks
===========================
The build argument ``pw_toolchain_STATIC_ANALYSIS_SKIP_SOURCES_RES`` is used
to exclude source files from the analysis. The list must contain regular
expressions matching individual files, rather than directories. For example,
provide ``"the_path/.*"`` to exclude all files in all directories under
``the_path``.

The build argument ``pw_toolchain_STATIC_ANALYSIS_SKIP_INCLUDE_PATHS`` is used
used to exclude header files from the analysis. This argument must be a list of
POSIX-style path suffixes for include paths, or regular expressions matching
include paths. For example, passing ``the_path/include`` excludes all header
files that are accessed from include paths ending in ``the_path/include``,
while passing ``.*/third_party/.*`` excludes all third-party header files.

Note that ``pw_toolchain_STATIC_ANALYSIS_SKIP_INCLUDE_PATHS`` operates on
include paths, not header file paths. For example, say your compile commands
include ``-Idrivers``, and this results in a file at ``drivers/public/i2c.h``
being included. You can skip this header by adding ``drivers`` or ``drivers.*``
to ``pw_toolchain_STATIC_ANALYSIS_SKIP_INCLUDE_PATHS``, but *not* by adding
``drivers/.*``: this last regex matches the header file path, but not the
include path.

Provided toolchains
===================
``pw_toolchain`` provides static analysis GN toolchains that may be used to
test host targets:

- pw_toolchain_host_clang.debug.static_analysis
- pw_toolchain_host_clang.speed_optimized.static_analysis
- pw_toolchain_host_clang.size_optimized.static_analysis
- pw_toolchain_host_clang.fuzz.static_analysis
  (if pw_toolchain_OSS_FUZZ_ENABLED is false)
- pw_toolchain_arm_clang.debug.static_analysis
- pw_toolchain_arm_clang.speed_optimized.static_analysis
- pw_toolchain_arm_clang.size_optimized.static_analysis

For example, to run ``clang-tidy`` on all source dependencies of the
``default`` target:

.. code-block::

   generate_toolchain("my_toolchain") {
     ..
     static_analysis = {
      enabled = true
     }
   }

   group("static_analysis") {
     deps = [ ":default(my_toolchain.static_analysis)" ]
   }

.. warning::

   The status of the static analysis checks might change when
   any relevant .clang-tidy file is updated. You should
   clean the output directory before invoking
   ``clang-tidy``.

-------------
Target traits
-------------
Pigweed targets expose a set of constants that describe properties of the target
or the toolchain compiling code for it. These are referred to as target traits.

In GN, these traits are exposed as GN args and are prefixed with
``pw_toolchain_`` (e.g. ``pw_toolchain_CXX_STANDARD``). They are defined in
``pw_toolchain/traits.gni``.

Traits must never be set by the user (e.g. with ``gn args``). Traits are always
set by the target.

.. warning::

   This feature is under development and is likely to change significantly.
   See `b/234883746 <http://issuetracker.google.com/issues/234883746>`_.

List of traits
==============
- ``CXX_STANDARD``. The C++ standard used by the toolchain. The value must be an
  integer value matching one of the standard values for the ``__cplusplus``
  macro. For example, ``201703`` corresponds to C++17. See
  https://en.cppreference.com/w/cpp/preprocessor/replace#Predefined_macros for
  further details.

---------------
C/C++ libraries
---------------
``pw_toolchain`` provides some toolchain-related C/C++ libraries.

``std:abort`` wrapper
=====================
The `std::abort <https://en.cppreference.com/w/cpp/utility/program/abort>`_
function is used to terminate a program abnormally. This function may be called
by standard library functions, so is often linked into binaries, even if users
never intentionally call it.

For embedded builds, the ``abort`` implementation likely does not work as
intended. For example, it may pull in undesired dependencies (e.g.
``std::raise``) and end in an infinite loop.

``pw_toolchain`` provides the ``pw_toolchain:wrap_abort`` library that replaces
``abort`` in builds where the default behavior is undesirable. It uses the
``-Wl,--wrap=abort`` linker option to redirect to ``abort`` calls to
``PW_CRASH`` instead.

arm-none-eabi-gcc support
=========================
Targets building with the GNU Arm Embedded Toolchain (``arm-none-eabi-gcc``)
should depend on the ``pw_toolchain/arm_gcc:arm_none_eabi_gcc_support`` library
into their builds. In GN, that target should be included in
``pw_build_LINK_DEPS``.

Newlib OS interface
-------------------
`Newlib <https://sourceware.org/newlib/>`_, the C Standard Library
implementation provided with ``arm-none-eabi-gcc``, defines a set of `OS
interface functions <https://sourceware.org/newlib/libc.html#Stubs>`_ that
should be implemented. Newlib provides default implementations, but using these
results in linker warnings like the following:

.. code-block:: none

   readr.c:(.text._read_r+0x10): warning: _read is not implemented and will always fail

Most of the OS interface functions should never be called in embedded builds.
The ``pw_toolchain/arg_gcc:newlib_os_interface_stubs`` library, which is
provided through ``pw_toolchain/arm_gcc:arm_none_eabi_gcc_support``, implements
these functions and forces a linker error if they are used. It also
automatically includes a wrapper for ``abort`` for use of ``stdout`` and
``stderr`` which abort if they are called.

If you need to use your own wrapper for ``abort``, include the library directly
using ``pw_toolchain/arm_gcc:newlib_os_interface_stubs``.

pw_toolchain/no_destructor.h
============================
.. doxygenclass:: pw::NoDestructor
