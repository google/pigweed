.. _module-pw_toolchain_bazel:

==================
pw_toolchain_bazel
==================

.. pigweed-module::
   :name: pw_toolchain_bazel
   :tagline: Modular Bazel C/C++ toolchain API
   :status: unstable
   :languages: Starlark

Assembling a complete, hermetic toolchain with Bazel using the native primitives
can be quite challenging. Additionally, Bazel's native API for declaring C/C++
toolchains doesn't inherently encourage modularity or reusability.

``pw_toolchain_bazel`` provides a suite of building blocks that make the process
of assembling a complete, hermetic toolchain significantly easier. The Bazel
rules introduced by this module push the vast majority of a toolchain's
declaration into build files, and encourages reusability through sharing of
flag groups, tools, and toolchain feature implementations.

While this module does **not** provide a hermetic toolchain, Pigweed provides
`fully instantiated and supported toolchains <https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain/host_clang/BUILD.bazel>`_
that are a useful reference for building your own toolchain.

.. warning::
   `b/309533028 <https://issues.pigweed.dev/309533028>`_\: This module is under
   construction and is subject to major breaking changes.

.. grid:: 1

   .. grid-item-card:: :octicon:`info` API reference
      :link: module-pw_toolchain_bazel-api
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Detailed reference information about the pw_toolchain_bazel API.

.. grid:: 1

   .. grid-item-card:: :octicon:`file` Original SEED
      :link: seed-0113
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      SEED-0113: Add modular Bazel C/C++ toolchain API

------------
Dependencies
------------
This module is not permitted to have dependencies on other modules. When this
module stabilizes, it will be broken out into a separate repository.

.. toctree::
   :hidden:
   :maxdepth: 1

   API reference <api>
