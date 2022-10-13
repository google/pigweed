.. _module-pw_ide:

------
pw_ide
------
This module provides tools for supporting code editor and IDE features for
Pigweed projects.

Usage
=====

Configuration
-------------
Pigweed IDE settings are stored in the project root in ``.pw_ide.yaml``, in
which these options can be configured:

* ``working_dir``: The working directory for compilation databases and caches
  (by default this is ``.pw_ide`` in the project root). This directory shouldn't
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
-----
Most of the time, ``pw ide setup`` is all you need to get started.

The working directory and other components can be created by running
``pw ide init``, although it is rarely necessary to run this command manually;
other subcommands will initialize if needed. You can also clear elements of the
working directory with ``pw ide clean``.

C++ Code Intelligence
---------------------
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
``clangd.sh`` instead of directly using the ``clangd`` binary.

``clangd`` must be run with arguments that provide the Pigweed environment paths
to the correct toolchains and sysroots. One way to do this is to launch your
editor from the terminal in an activated environment (e.g. running ``vim`` from
the terminal), in which case nothing special needs to be done as long as your
toolchains are in the Pigweed environment or ``$PATH``. But if you launch your
editor outside of the activated environment (e.g. launching Visual Studio Code
from your GUI shell's launcher), you can generate the command that invokes
``clangd`` with the right arguments with:

.. code-block:: bash

   pw ide cpp --clangd-command

Python Code Intelligence
------------------------
Any Python language server should work well with Pigweed projects as long as
it's configured to use the Pigweed virtual environment. You can output the path
to the virtual environment on your system with:

.. code-block:: bash

  pw ide python --venv

Design
======

Supporting ``clangd`` for Embedded Projects
-------------------------------------------
There are three main challenges that often prevent ``clangd`` from working
out-of-the-box with embedded projects:

#. Embedded projects cross-compile using alternative toolchains, rather than
   using the system toolchain. ``clangd`` doesn't know about those toolchains
   by default.

#. Embedded projects (particularly Pigweed project) often have *multiple*
   targets that use *multiple* toolchains. Most build systems that generate
   compilation databases put all compile commands in a single database, meaning
   a single file can have multiple, conflicting compile commands. ``clangd``
   will typically use the first one it finds, which may not be the one you want.

#. Pigweed projects have build steps that use languages other than C/C++. These
   steps are not relevant to ``clangd`` but many build systems will include them
   in the compilation database anyway.

To deal with these challenges, ``pw_ide`` processes the compilation database you
provide, yielding one or more compilation databases that are valid, consistent,
and specific to a particular build target. This enables code intelligence and
navigation features that reflect that build.

After processing a compilation database, ``pw_ide`` knows what targets are
available and provides tools for selecting which target is active. These tools
can be integrated into code editors, but are ultimately CLI-driven and
editor-agnostic. Enabling code intelligence in your editor may be as simple as
configuring its language server protocol client to use the ``clangd`` command
that ``pw_ide`` can generate for you.

When to provide additional configuration to support your use cases
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The default configuration for ``clangd`` in ``pw_ide`` should work without
additional configuration as long as you're using only toolchains provided by
Pigweed and your native host toolchain. If you're using other toolchains, keep
reading.

``clangd`` needs two pieces of information to use a toolchain:

#. A path to the compiler, which will be taken from the compile command.

#. The same compiler to be reflected in the
   `query driver <https://releases.llvm.org/10.0.0/tools/clang/tools/extra/docs/clangd/Configuration.html>`_
   argument provided when running ``clangd``.

When using ``pw_ide`` with external toolchains, you only need to add a path to
the compiler to ``clangd_additional_query_drivers`` in your project's
``pw_ide.yaml`` file. When processing a compilation database, ``pw_ide`` will
use the query driver globs to find your compiler and configure ``clangd`` to
use it.

Selected API Reference
======================
.. automodule:: pw_ide.cpp
   :members: CppCompileCommand, CppCompilationDatabase, CppCompilationDatabasesMap, CppIdeFeaturesState, path_to_executable, target_is_enabled, ClangdSettings
