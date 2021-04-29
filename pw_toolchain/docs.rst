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

 - arm_gcc_cortex_m4_og
 - arm_gcc_cortex_m4_o1
 - arm_gcc_cortex_m4_o2
 - arm_gcc_cortex_m4_os
 - arm_gcc_cortex_m4f_og
 - arm_gcc_cortex_m4f_o1
 - arm_gcc_cortex_m4f_o2
 - arm_gcc_cortex_m4f_os
 - host_clang_og
 - host_clang_o2
 - host_clang_os
 - host_gcc_og
 - host_gcc_o2
 - host_gcc_os

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
