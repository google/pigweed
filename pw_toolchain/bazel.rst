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
