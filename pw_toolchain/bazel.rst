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

Layering check
==============
Upstream Pigweed toolchains have support for `layering check
<https://maskray.me/blog/2022-09-25-layering-check-with-clang>`__. In short,
enabling layering check makes it a compile-time error to ``#include`` a header
that's not in the ``hdrs`` of a ``cc_library`` you directly depend on. This
produces cleaner dependency graphs and is recommended for all users.

.. admonition:: Note

   Layering check requires Bazel 8.0.0 or newer.


How to enable layering check?
-----------------------------
#. Add ``common --//pw_toolchain/host_clang:layering_check`` to your
   ``.bazelrc``. This does not by itself enable the check, it only instructs
   Bazel to include support for it in the toolchain configuration. (This flag
   will become true by default and be removed once all known Pigweed users are
   on Bazel 8.)
#. Enable the ``layering_check`` feature to enable enforcement. The recommended
   way to do this is to add a `REPO.bazel
   <https://bazel.build/external/overview#repo.bazel>`__ file at the root of
   your project, with the following content:

   .. code-block:: py

      repo(
          features = ["layering_check"],
      )

   This will enable ``layering_check`` for all code in your project, but not for
   code coming from any external dependencies also built with Bazel.

Gradual rollout
---------------
When you enable layering check for the first time, you will likely get a very
large number of errors, since many packages in your project will contain
layering violations. If there are too many errors to fix at once, here's one way
to roll out ``layering_check`` gradually:

#. Enable the ``layering_check`` feature for the entire repo, as discussed
   above.
#. In the same commit, disable layering check for each individual package
   (``BUILD.bazel`` file). This can be done using the following `buildozer
   <https://github.com/bazelbuild/buildtools/blob/main/buildozer/README.md>`__
   one-liner:

   .. code-block:: console

      buildozer 'add features -layering_check' '//...:__pkg__'

   Note the ``-`` in front of ``layering_check`` in the command above!

#. Re-enable layering check package by package by removing the lines added by
   buildozer. When no ``-layering_check`` remains in your codebase, you've
   enabled layering check for the entire repo!

You can also enable or disable the ``layering_check`` feature for individual
targets, using the `features
<https://bazel.build/reference/be/common-definitions#common.features>`__
attribute.

Limitations
-----------
#. Layering check only applies to ``#include`` statements in source files, not
   in header files. To also apply it to header files, you need the
   ``parse_headers`` feature. Pigweed's toolchains do not yet contain its
   implementation. Adding it is tracked at :bug:`391367050`.
#. Layering check will not prevent you from using symbols from transitively
   included headers. For this, use `misc-include-cleaner
   <https://clang.llvm.org/extra/clang-tidy/checks/misc/include-cleaner.html>`__
   in clang-tidy.  See also :bug:`329671260`.

.. _module-pw_toolchain-bazel-compiler-specific-logic:

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
