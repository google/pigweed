.. _module-pw_toolchain_bazel:

==================
pw_toolchain_bazel
==================
This module provides building blocks for Bazel's ``cc_toolchain`` API in a way
that increases modularity and reusability. While this module does NOT provide a
hermetic toolchain, Pigweed does provide fully instantiated and supported
toolchains as part of ``pw_toolchain``.

.. warning::
   This module is under construction and is subject to major breaking changes.

------------
Dependencies
------------
This module is not permitted to have dependencies on other modules. When this
module stabilizes, it will be broken out into a separate repository.
