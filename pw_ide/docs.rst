.. _module-pw_ide:

------
pw_ide
------
This module provides tools for supporting code editor and IDE features for
Pigweed projects.

Configuration
=============
Pigweed IDE settings are stored in the project root in ``.pw_ide.yaml``, in
which these options can be configured:

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

* ``setup``: Projects can define a set of steps that automatically set up IDE
  features with sensible defaults.

Setup
=====
Most of the time, ``pw ide setup`` is all you need to get started.

The working directory and other components can be created by running
``pw ide init``, although it is rarely necessary to run this command manually;
other subcommands will initialize if needed. You can also clear elements of the
working directory with ``pw ide clean``.

C++ Code Intelligence via ``clangd``
====================================
`clangd <https://clangd.llvm.org/>`_ is a language server that provides C/C++
code intelligence features to any editor that supports the language server
protocol (LSP). It uses a
`compilation database <https://clang.llvm.org/docs/JSONCompilationDatabase.html>`_,
a JSON file containing the compile commands for the project. Projects that have
multiple targets and/or use multiple toolchains need separate compilation
databases for each target/toolchain. ``pw_ide`` provides tools for managing
those databases.

Assuming you have a compilation database output from a build system, start with:

.. code-block:: bash

  pw ide cpp --process <path to your compile_commands.json>

The ``pw_ide`` working directory will now contain one or more compilation
database files, each for a separate target among the targets defined in
``.pw_ide.yaml``. List the available targets with:

.. code-block:: bash

  pw ide cpp --list

Then set the target that ``clangd`` should use with:

.. code-block:: bash

  pw ide cpp --set <selected target name>

``clangd`` can now be configured to point to the ``compile_commands.json`` file
in the ``pw_ide`` working directory and provide code intelligence for the
selected target. If you select a new target, ``clangd`` *does not* need to be
reconfigured to look at a new file (in other words, ``clangd`` can always be
pointed at the same, stable ``compile_commands.json`` file). However,
``clangd`` may need to be restarted when the target changes.

``clangd`` must be run within the activated Pigweed environment in order for
correct toolchain paths and sysroots to be detected. If you launch your editor
from the terminal in an activated environment, then nothing special needs to be
done (e.g. running ``vim`` from the terminal). But if you launch your editor
outside of the activated environment (e.g. launching Visual Studio Code from
your GUI shell's launcher), you will need to use the included wrapper
``clangd.sh`` instead of directly using the ``clangd`` binary.
