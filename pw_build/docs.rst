.. _chapter-build:

.. default-domain:: cpp

.. highlight:: sh

--------
pw_build
--------
The build module contains the configuration necessary to build Pigweed using
either `GN`_/`Ninja`_ or `Bazel`_.

.. _GN: https://gn.googlesource.com/gn/
.. _Ninja: https://ninja-build.org/
.. _Bazel: https://bazel.build/

GN / Ninja
==========
The common configuration for GN for all modules is in the ``BUILD.gn`` file.
It contains ``config`` declarations referenced by ``BUILD.gn`` files in other
modules.

The ``pw_executable.gni`` file contains a wrapper to build a target for the
globally-defined target type, and ``python_script.gni`` makes it easy to run
Python scripts as part of build rules.

Bazel
=====
The common configuration for Bazel for all modules is in the ``pigweed.bzl``
file. The built-in Bazel rules ``cc_binary``, ``cc_library``, and ``cc_test``
are wrapped with ``pw_cc_binary``, ``pw_cc_library``, and ``pw_cc_test``.
These wrappers add parameters to calls to the compiler and linker.

The ``BUILD`` file is merely a placeholder and currently does nothing.
