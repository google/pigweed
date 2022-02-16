.. _module-pw_toolchain:

------------
pw_toolchain
------------
GN toolchains function both as a set of tools for compilation and as a workspace
for evaluating build files. The same compilations and actions can be executed by
different toolchains. Each toolchain maintains its own set of build args, and
build steps from all toolchains can be executed in parallel.

Toolchains
==========
``pw_toolchain`` provides GN toolchains that may be used to build Pigweed. The
following toolchains are defined:

 - pw_toolchain_arm_clang.cortex_m0plus_debug
 - pw_toolchain_arm_clang.cortex_m0plus_speed_optimized
 - pw_toolchain_arm_clang.cortex_m0plus_size_optimized
 - pw_toolchain_arm_clang.cortex_m3_debug
 - pw_toolchain_arm_clang.cortex_m3_speed_optimized
 - pw_toolchain_arm_clang.cortex_m3_size_optimized
 - pw_toolchain_arm_clang.cortex_m4_debug
 - pw_toolchain_arm_clang.cortex_m4_speed_optimized
 - pw_toolchain_arm_clang.cortex_m4_size_optimized
 - pw_toolchain_arm_clang.cortex_m4f_debug
 - pw_toolchain_arm_clang.cortex_m4f_speed_optimized
 - pw_toolchain_arm_clang.cortex_m4f_size_optimized
 - pw_toolchain_arm_gcc.cortex_m0plus_debug
 - pw_toolchain_arm_gcc.cortex_m0plus_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m0plus_size_optimized
 - pw_toolchain_arm_gcc.cortex_m3_debug
 - pw_toolchain_arm_gcc.cortex_m3_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m3_size_optimized
 - pw_toolchain_arm_gcc.cortex_m4_debug
 - pw_toolchain_arm_gcc.cortex_m4_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m4_size_optimized
 - pw_toolchain_arm_gcc.cortex_m4f_debug
 - pw_toolchain_arm_gcc.cortex_m4f_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m4f_size_optimized
 - pw_toolchain_arm_gcc.cortex_m7_debug
 - pw_toolchain_arm_gcc.cortex_m7_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m7_size_optimized
 - pw_toolchain_arm_gcc.cortex_m7f_debug
 - pw_toolchain_arm_gcc.cortex_m7f_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m7f_size_optimized
 - pw_toolchain_arm_gcc.cortex_m33_debug
 - pw_toolchain_arm_gcc.cortex_m33_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m33_size_optimized
 - pw_toolchain_arm_gcc.cortex_m33f_debug
 - pw_toolchain_arm_gcc.cortex_m33f_speed_optimized
 - pw_toolchain_arm_gcc.cortex_m33f_size_optimized
 - pw_toolchain_host_clang.debug
 - pw_toolchain_host_clang.speed_optimized
 - pw_toolchain_host_clang.size_optimized
 - pw_toolchain_host_clang.fuzz
 - pw_toolchain_host_gcc.debug
 - pw_toolchain_host_gcc.speed_optimized
 - pw_toolchain_host_gcc.size_optimized

 .. note::
  The documentation for this module is currently incomplete.

Non-C/C++ toolchains
====================
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

Testing other compiler versions
===============================
The clang-based toolchain provided by Pigweed can be substituted with another
version by modifying the ``pw_toolchain_CLANG_PREFIX`` GN build argument to
point to the directory that contains the desired clang, clang++, and llvm-ar
binaries. This should only be used for debugging purposes. Pigweed does not
officially support any compilers other than those provided by Pigweed.

Running static analysis checks
==============================
``clang-tidy`` can be run as a compiler replacement, to analyze all sources
built for a target. ``pw_toolchain/static_analysis_toolchain.gni`` provides
the ``pw_static_analysis_toolchain`` template. This template creates toolchains
that execute ``clang-tidy`` for C/C++ sources, and mock implementations of
the ``link``, ``alink`` and ``solink`` tools.

Additionally, ``generate_toolchain`` implements a boolean flag
``static_analysis`` (default ``false``) which generates the derived
toolchain ``${target_name}.static_analysis`` using
``pw_generate_static_analysis_toolchain`` and the toolchain options.

Excluding files from checks
---------------------------
The build argument ``pw_toolchain_STATIC_ANALYSIS_SKIP_SOURCES_RES`` is used
used to exclude source files from the analysis. The list must contain regular
expressions matching individual files, rather than directories. For example,
provide ``"the_path/.*"`` to exclude all files in all directories under
``the_path``.

The build argument ``pw_toolchain_STATIC_ANALYSIS_SKIP_INCLUDE_PATHS`` is used
used to exclude header files from the analysis. This argument must be a list of
POSIX-style path suffixes for include paths, or regular expressions. For
example, passing ``the_path/include`` excludes all header files that are
accessed from include paths ending in ``the_path/include``, while passing
``.*/third_party/.*`` excludes all third-party header files.

Provided toolchains
-------------------
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
    static_analysis = true
  }

  group("static_analysis") {
    deps = [ ":default(my_toolchain.static_analysis)" ]
  }

.. warning::
    The status of the static analysis checks might change when
    any relevant .clang-tidy file is updated. You should
    clean the output directory before invoking
    ``clang-tidy``.
