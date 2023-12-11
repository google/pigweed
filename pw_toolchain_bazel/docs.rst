.. _module-pw_toolchain_bazel:

==================
pw_toolchain_bazel
==================

.. pigweed-module::
   :name: pw_toolchain_bazel
   :tagline: A modular toolkit for declaring C/C++ toolchains in Bazel
   :status: unstable
   :languages: Starlark

   - **Modular**: Construct your own toolchain using shared building blocks, and
     share common flags between multiple platforms. You can even share toolchain
     components and definitions across the boundaries of different projects!
   - **Declarative**: Toolchain configurations are centralized to a single
     source-of-truth.

Assembling a complete, hermetic C/C++ toolchain using Bazel's native primitives
can be quite challenging. Additionally, the native API for declaring C/C++
toolchains doesn't inherently encourage modularity or reusability, and lacks
consistent documentation.

``pw_toolchain_bazel`` provides a suite of building blocks that make the process
of assembling a complete, hermetic toolchain significantly easier. The Bazel
rules introduced by this module push the vast majority of a toolchain's
declaration into build files, and encourages reusability through sharing of
flag groups, tools, and toolchain feature implementations.

Why create `hermetic toolchains <https://bazel.build/basics/hermeticity>`_?

- **Eliminate "it compiles on my machine."** The same toolchain across every
  workstation means the same flags, the same warnings, and the same errors.
- **Produce the same binary every time.** By keeping the toolchain and
  associated flags consistent and isolated across different environments, you
  can expect your final firmware images to be the same no matter where they're
  compiled.
- **Correctly support incremental builds.** When the system environment affects
  how tools behave, incremental builds can produce misconfigured or
  fundamentally broken binaries.

While this module does **not** provide a hermetic toolchain, Pigweed provides
`fully instantiated and supported toolchains <https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain/host_clang/BUILD.bazel>`_
that are a useful reference for building your own toolchain.

.. warning::
   `b/309533028 <https://issues.pigweed.dev/309533028>`_\: This module is under
   construction, and is missing a few significant features.

.. grid:: 1

   .. grid-item-card:: :octicon:`rocket` Get started
      :link: module-pw_toolchain_bazel-get-started
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Set up a custom C/C++ toolchain project.

.. grid:: 2

   .. grid-item-card:: :octicon:`info` API reference
      :link: module-pw_toolchain_bazel-api
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Detailed reference information about the pw_toolchain_bazel API.

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
   Get Started <get_started>
