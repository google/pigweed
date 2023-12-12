.. _module-pw_toolchain_bazel-get-started:

===================================
Get started with pw_toolchain_bazel
===================================
.. pigweed-module-subpage::
   :name: pw_toolchain_bazel
   :tagline: A modular toolkit for declaring C/C++ toolchains in Bazel

-----------
Quick start
-----------
The fastest way to get started using ``pw_toolchain_bazel`` is to use Pigweed's
upstream toolchains.

1. Enable required features in your project's ``//.bazelrc`` file:

   .. code-block:: sh

      # Required for new toolchain resolution API.
      build --incompatible_enable_cc_toolchain_resolution

      # Do not attempt to configure an autodetected (local) toolchain.
      common --repo_env=BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1

2. Configure ``pw_toolchain_bazel`` in your project's ``//WORKSPACE`` file:

   .. code-block:: py

      # Add Pigweed itself, as a submodule from `//third_party/pigweed`.
      #
      # TODO: b/300695111 - Support depending on Pigweed as a git_repository,
      # even if you use pw_toolchain.
      local_repository(
          name = "pigweed",
          path = "third_party/pigweed",
      )
      local_repository(
          name = "pw_toolchain",
          path = "third_party/pigweed/pw_toolchain_bazel",
      )

      # Set up CIPD.
      load(
          "@pigweed//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl",
          "cipd_client_repository",
          "cipd_repository",
      )

      cipd_client_repository()

      # Set up and register Pigweed's toolchains.
      load(
          "@pigweed//pw_toolchain:register_toolchains.bzl",
          "register_pigweed_cxx_toolchains"
      )

      register_pigweed_cxx_toolchains()

And you're done! You should now be able to compile for macOS, Linux, and ARM
Cortex-M devices.

.. _module-pw_toolchain_bazel-get-started-overview:

--------
Overview
--------
This guide shows you how to use ``pw_toolchain_bazel`` to assemble a fully
working toolchain. There are three core elements in a C/C++ toolchain in
Bazel:

#. The underlying tools used to perform compile and link actions.
#. Flag declarations that may or may not apply to a given toolchain
   configuration.
#. The final toolchain definition that binds tools and flag declarations
   together to produce working C/C++ compile and link commands.

This guide assumes you have a good grasp on writing Bazel build files, and also
assumes you have a working understanding of what flags are typically passed to
various compile and link tool invocations.

--------------------------------
Adding Pigweed to your WORKSPACE
--------------------------------
Before you can use Pigweed and ``pw_toolchain_bazel`` in your project, you must
register Pigweed in your ``//WORKSPACE`` file:

.. code-block:: py

   # Add Pigweed itself, as a submodule from `//third_party/pigweed`.
   #
   # TODO: b/300695111 - Support depending on Pigweed as a git_repository,
   # even if you use pw_toolchain.
   local_repository(
       name = "pigweed",
       path = "third_party/pigweed",
   )
   local_repository(
       name = "pw_toolchain",
       path = "third_party/pigweed/pw_toolchain_bazel",
   )

.. admonition:: Note

   `b/300695111 <https://issues.pigweed.dev/300695111>`_\: You must add Pigweed
   as a submodule to use Pigweed in a Bazel project. Pigweed does not yet work
   when added as a ``http_repository``.

------------------
Configure .bazelrc
------------------
To use this module's toolchain rules, you must first add a couple
flags that tell Bazel how to find toolchain definitions. Bazel's ``.bazelrc``
lives at the root of your project, and is the source of truth for your
project-specific build flags that control Bazel's behavior.

.. code-block:: sh

   # Required for new toolchain resolution API.
   build --incompatible_enable_cc_toolchain_resolution

   # Do not attempt to configure an autodetected (local) toolchain. We vendor
   # all our toolchains, and CI VMs may not have any local toolchain to detect.
   common --repo_env=BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1

.. _module-pw_toolchain_bazel-assemble-a-tool-suite:

---------------------
Assemble a tool suite
---------------------
The fastest way to get started is using a toolchain tool repository template.
``pw_toolchain_bazel`` provides pre-assembled templates for ``clang`` and
``arm-none-eabi-gcc`` toolchains in the
`@pw_toolchain//build_external <https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain_bazel/build_external/>`_
package. These build files can be attached to an external repository in your
``WORKSPACE`` file using the ``build_file`` attribute of ``http_archive``,
``git_repository``, or ``cipd_repository``.

.. code-block:: py

   # Declare a toolchain tool suite for Linux.
   http_archive(
       name = "linux_clang_toolchain",
       build_file = "@pw_toolchain//build_external:llvm_clang.BUILD",
       sha256 = "884ee67d647d77e58740c1e645649e29ae9e8a6fe87c1376be0f3a30f3cc9ab3",
       strip_prefix = "clang+llvm-17.0.6-x86_64-linux-gnu-ubuntu-22.04",
       url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/clang+llvm-17.0.6-x86_64-linux-gnu-ubuntu-22.04.tar.xz",
   )

---------------------------
Create toolchain definition
---------------------------
To set up a complete toolchain definition, you'll need ``toolchain`` and
``pw_cc_toolchain`` rules that serve as the core of your toolchain.
A simplified example is provided below.

.. code-block:: py

   load("@pw_toolchain//cc_toolchain:defs.bzl", "pw_cc_toolchain")

   pw_cc_toolchain(
       name = "host_toolchain",
       abi_libc_version = "unknown",
       abi_version = "unknown",
       action_config_flag_sets = [
           "@pw_toolchain//flag_sets:c++17",
           "@pw_toolchain//flag_sets:debugging",
           "@pw_toolchain//flag_sets:no_canonical_prefixes",
       ],
       action_configs = [
           "@llvm_toolchain//:ar",
           "@llvm_toolchain//:clang",
           "@llvm_toolchain//:clang++",
           "@llvm_toolchain//:lld",
           "@llvm_toolchain//:llvm-cov",
           "@llvm_toolchain//:llvm-objcopy",
           "@llvm_toolchain//:llvm-objdump",
           "@llvm_toolchain//:llvm-strip",
       ],
       compiler = "unknown",
       cxx_builtin_include_directories = [
           "%package(@llvm_toolchain//)%/include/x86_64-unknown-linux-gnu/c++/v1",
           "%package(@llvm_toolchain//)%/include/c++/v1",
           "%package(@llvm_toolchain//)%/lib/clang/17/include",
       ],
       host_system_name = "unknown",
       supports_param_files = 0,
       target_cpu = "unknown",
       target_libc = "unknown",
       target_system_name = "unknown",
       toolchain_identifier = "host-linux-toolchain",
   )

   toolchain(
       name = "host_cc_toolchain_linux",
       # This is the list of constraints that must be satisfied for the suite of
       # toolchain tools to be determined as runnable on the current machine.
       exec_compatible_with = [
           "@platforms//os:linux",
       ],
       # This is the list of constraints that dictates compatibility of the final
       # artifacts produced by this toolchain.
       target_compatible_with = [
           "@platforms//os:linux",
       ],
       toolchain = ":host_toolchain",
       toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
   )

.. admonition:: Note

   `b/315206506 <https://issues.pigweed.dev/315206506>`_\: The fields set to
   ``"unknown"`` are currently mandatory, but may not be strictly required in
   the future.

The ``toolchain`` rule
======================
The ``toolchain`` rule communicates to Bazel what kind of toolchains are
available, what environments the tools can run on, and what environment the
artifacts are intended for. A quick overview of the critical parts of this
rule are outlined below.

- ``name``: The name of the toolchain rule. This is the label that you
  reference when registering a toolchain so Bazel knows it may use this
  toolchain.
- ``toolchain_type``: The language this toolchain is designed for. Today,
  ``pw_toolchain_bazel`` only supports C/C++ toolchains via the
  ``@bazel_tools//tools/cpp:toolchain_type`` type.
- ``exec_compatible_with``: What constraints must be satisfied for this
  toolchain to be compatible with the execution environment. In simpler terms,
  if the machine that is currently running the build is a Linux x86_64 machine,
  it can only use toolchains designed to run on that OS and architecture.
  ``exec_compatible_with`` is what prevents a Linux machine from trying to
  compile using tools designed for a Windows machine and vice versa.
- ``target_compatible_with``: What constraints must be satisfied for this
  toolchain to be compatible with the targeted environment. Rather than
  specifying whether the *tools* are compatible, this specifies the
  compatibility of the final artifacts produced by this toolchain.
  For example, ``target_compatible_with`` is what tells Bazel that a toolchain
  is building firmware for a Cortex-M4.
- ``toolchain``: The rule that implements the toolchain behavior. When using
  ``pw_toolchain_bazel``, this points to a :py:class:`pw_cc_toolchain` rule.
  Multiple ``toolchain`` rules can point to the same
  :py:class:`pw_cc_toolchain`, which can be useful for creating parameterized
  toolchains that have a lot in common.


The ``pw_cc_toolchain`` rule
============================
This is the heart of your C/C++ toolchain configuration, and has two main
configuration surfaces of interest.

- :py:attr:`pw_cc_toolchain.action_configs`\: This is a list of bindings that
  map various toolchain actions to the appropriate tools. Typically you'll just
  want to list all of the :py:class:`pw_cc_action_config` rules included in your
  toolchain repository from
  :ref:`module-pw_toolchain_bazel-assemble-a-tool-suite`. If you need to swap
  out a particular tool, you can just create a custom
  :py:class:`pw_cc_tool` and :py:class:`pw_cc_action_config` and list it here.
- :py:attr:`pw_cc_toolchain.action_config_flag_sets`\: This lists all the flags
  that are applied when compiling with this toolchain. Each
  :py:class:`pw_cc_flag_set` listed here includes at least one flag that applies
  to at least one kind of action.

While the other attributes of a :py:class:`pw_cc_toolchain` are still required,
their behaviors are less interesting from a configuration perspective and are
required for correctness and completeness reasons. See the full
API reference for :py:class:`pw_cc_toolchain` for more information.

-----------------------
Register your toolchain
-----------------------
Once you've declared a complete toolchain to your liking, you'll need to
register it in your project's ``WORKSPACE`` file so Bazel knows it can use the
new toolchain. An example for a ``toolchain`` with the name
``host_cc_toolchain_linux`` living in ``//toolchains/BUILD`` is illustrated
below.

.. code-block:: py

   register_toolchains(
        "//toolchains:host_cc_toolchain_linux",
   )

At this point, you should have a custom, working toolchain! For more extensive
examples, consider taking a look at Pigweed's
`fully instantiated and supported toolchains <https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain/host_clang/BUILD.bazel>`_

---------------------------------
Customize behavior with flag sets
---------------------------------
Now that your toolchain is working, you can customize it by introducing new flag
sets.

Configure warnings
==================
Enabling compiler warnings and setting them to be treated as errors is a great
way to prevent unintentional bugs that stem from dubious code.

.. code-block:: py

   load(
       "@pw_toolchain//cc_toolchain:defs.bzl",
       "ALL_CPP_COMPILER_ACTIONS",
       "ALL_C_COMPILER_ACTIONS",
       "pw_cc_flag_set",
   )

   pw_cc_flag_set(
       name = "warnings",
       actions = ALL_C_COMPILER_ACTIONS + ALL_CPP_COMPILER_ACTIONS,
       flags = [
           "-Wall",
           "-Wextra",
           "-Werror",  # Make all warnings errors, except for the exemptions below.
           "-Wno-error=cpp",  # preprocessor #warning statement
           "-Wno-error=deprecated-declarations",  # [[deprecated]] attribute
       ],
   )

Omit unreferenced symbols
=========================
If a function, variable, or data section isn't used anywhere in your binaries,
it can be omitted with the following flag sets.

.. code-block:: py

   load(
       "@pw_toolchain//cc_toolchain:defs.bzl",
       "ALL_CPP_COMPILER_ACTIONS",
       "ALL_C_COMPILER_ACTIONS",
       "ALL_LINK_ACTIONS",
       "pw_cc_flag_set",
   )

   # Treats symbols representing functions and data as individual sections.
   # This is mostly relevant when using `:omit_unused_sections`.
   pw_cc_flag_set(
       name = "function_and_data_sections",
       actions = ALL_C_COMPILER_ACTIONS + ALL_CPP_COMPILER_ACTIONS,
       flags = [
           "-ffunction-sections",
           "-fdata-sections",
       ],
   )

   pw_cc_flag_set(
       name = "omit_unused_sections",
       actions = ALL_LINK_ACTIONS,
       # This flag is parameterized by operating system. macOS and iOS require
       # a different flag to express this concept.
       flags = select({
           "@platforms//os:macos": ["-Wl,-dead_strip"],
           "@platforms//os:ios": ["-Wl,-dead_strip"],
           "//conditions:default": ["-Wl,--gc-sections"],
       }),
   )

Set global defines
==================
Toolchains may declare preprocessor defines that are available for all compile
actions.

.. code-block:: py

   load(
       "@pw_toolchain//cc_toolchain:defs.bzl",
       "ALL_ASM_COMPILER_ACTIONS",
       "ALL_CPP_COMPILER_ACTIONS",
       "ALL_C_COMPILER_ACTIONS",
       "pw_cc_flag_set",
   )

   # Specify global defines that should be available to all compile actions.
   pw_cc_flag_set(
      name = "global_defines",
      actions = ALL_ASM_COMPILER_ACTIONS + ALL_C_COMPILER_ACTIONS + ALL_CPP_COMPILER_ACTIONS,
      flags = [
         "-DPW_LOG_LEVEL=PW_LOG_LEVEL_INFO",  # Omit all debug logs.
      ],
   )

Bind custom flags to a toolchain
================================
After you've assembled a selection of custom flag sets, you can bind them to
your toolchain definition by listing them in
:py:attr:`pw_cc_toolchain.action_config_flag_sets`\:

.. code-block:: py
   :emphasize-lines: 9,10,11,12

   pw_cc_toolchain(
       name = "host_toolchain",
       abi_libc_version = "unknown",
       abi_version = "unknown",
       action_config_flag_sets = [
           "@pw_toolchain//flag_sets:c++17",
           "@pw_toolchain//flag_sets:debugging",
           "@pw_toolchain//flag_sets:no_canonical_prefixes",
           ":warnings",  # Newly added pw_cc_flag_set from above.
           ":function_and_data_sections",  # Newly added pw_cc_flag_set from above.
           ":omit_unused_sections",  # Newly added pw_cc_flag_set from above.
           ":global_defines",  # Newly added pw_cc_flag_set from above.
       ],
       action_configs = [
           "@llvm_toolchain//:ar",
           "@llvm_toolchain//:clang",
           "@llvm_toolchain//:clang++",
       ...
   )

.. admonition:: Note

   Flags appear in the tool invocations in the order as they are listed
   in :py:attr:`pw_cc_toolchain.action_config_flag_sets`\, so if you need a flag
   to appear earlier in the command-line invocation of the tool just move it to
   towards the beginning of the list.

You can use ``pw_cc_flag_set`` rules to add support for new architectures,
enable/disable warnings, add preprocessor defines, enable LTO, and more.
