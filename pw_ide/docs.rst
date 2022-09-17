.. _module-pw_ide:

------
pw_ide
------
This module provides tools for supporting code editor and IDE features for
Pigweed projects.

Configuration
=============
Pigweed IDE settings are stored in the project root in ``.pw_ide.yaml``, in which
these options can be configured:

* ``working_dir``: The working directory for compilation databases and caches
  (by default this is `.pw_ide` in the project root). This directory shouldn't
  be committed to your repository or be a directory that is routinely deleted or
  manipulated by other processes.

* ``targets``: A list of build targets to use for code analysis, which is likely
  to be a subset of the project's total targets. The target name needs to match
  the name of the directory that holds the build system artifacts for the
  target. For example, GN outputs build artifacts for the
  ``pw_strict_host_clang_debug`` target in a directory with that name in the
  ``out`` directory. So that becomes the canonical name for that target.
