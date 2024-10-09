.. _module-pw_toolchain-bazel:

===============================
Bazel build system integrations
===============================
Pigweed provides a suite of Bazel build integrations to compliment existing
Bazel toolchain constructs such as `rules_cc toolchains <https://github.com/bazelbuild/rules_cc/blob/main/cc/toolchains/README.md>`_
to make it easier to design robust, feature-rich toolchains.

.. _module-pw_toolchain-bazel-upstream-pigweed-toolchains:

---------------------------
Upstream Pigweed toolchains
---------------------------
Pigweed's C/C++ toolchains are automatically registered when using Pigweed from
a Bzlmod Bazel project. Legacy WORKSPACE-based projects can use Pigweed's
upstream toolchains by calling ``register_pigweed_cxx_toolchains()``:

.. code-block:: py

   load("@pigweed//pw_toolchain:register_toolchains.bzl", "register_pigweed_cxx_toolchains")

   register_pigweed_cxx_toolchains()


.. admonition:: Note
   :class: warning

   Pigweed's upstream toolchains are subject to change without notice. If you
   would prefer more stability in toolchain configurations, consider declaring
   custom toolchains in your project.

-----------------------------
Compiler-specific build logic
-----------------------------
Whenever possible, avoid introducing compiler-specific behaviors in Bazel
``BUILD`` files. Instead, prefer to design build logic against
more intentional :ref:`docs-bazel-compatibility`. For compiler-specific
behavior, this means defining and/or using compiler capabilities like
`@rules_cc//cc/toolchains/capabilities:supports_interface_shared_libraries <https://github.com/bazelbuild/rules_cc/blob/main/cc/toolchains/capabilities/BUILD>`__

If you need to expose a toolchain capability as a choice in a select, you
can use ``pw_cc_toolchain_feature_is_enabled``.

Example:

.. code-block:: py

   load(
       "@pigweed//pw_toolchain/cc/current_toolchain:pw_cc_toolchain_feature_is_enabled.bzl",
       "pw_cc_toolchain_feature_is_enabled",
   )

   pw_cc_toolchain_feature_is_enabled(
       name = "llvm_libc_enabled",
       feature_name = "llvm_libc",
   )

   cc_library(
       name = "libfoo",
       deps = select({
           ":llvm_libc_enabled": ["//foo:llvm_libc_extras"],
           "//conditions:default": [],
       }),
   )

If you absolutely must introduce a ``select`` statement that checks the current
compiler, use Pigweed's helper macros.

Example:

.. code-block:: py

   load(
       "@pigweed//pw_toolchain/cc/current_toolchain:conditions.bzl",
       "if_compiler_is_clang",
       "if_linker_is_gcc",
   )

   cc_library(
       copts = if_compiler_is_clang(
           ["-fno-codegen"],
           default = [],
       ),
       linkopts = if_linker_is_gcc(
           ["-Wl,--delete-main"],
           default = [],
       ),
       srcs = ["lib.cc"],
   )
