.. _docs-editors:

-------------------
Code Editor Support
-------------------
Pigweed projects can be (and have been!) successfully developed in simple text
editors. However, if you want to take advantage of the latest code intelligence
tools and IDE features, those can be configured to work great with Pigweed.

We are building the ideal development experience for Pigweed projects using
`Visual Studio Code <https://code.visualstudio.com/>`_, a powerful and highly
extensible code editor. However, most of the features described in this doc are
available to other popular editors; you may just need to do a little more
configuration to make it work.

Quick Start for Contributing to Pigweed
=======================================
If you're developing on upstream Pigweed, you can use Visual Studio Code, which
is already configured to work best with Pigweed, or you can use another editor
and configure it yourself.

With Visual Studio Code
------------------------
There are three quick steps to get started with Pigweed in Visual Studio Code.

1. Bootstrap your Pigweed environment (if you haven't already).

2. Run ``gn gen out`` (if you haven't already).

3. Run ``pw ide sync``

4. Open the Pigweed repository in Visual Studio Code and install the recommended
   plugins.

You're done! Refer to the ``pw_ide`` documentation for
:ref:`Visual Studio Code<module-pw_ide-vscode>` for more guidance.

With Other Editors
-------------------
There are three short steps to get code intelligence in most editors for C++ and
Python, the two most prevalent languages in Pigweed.

1. Bootstrap your Pigweed environment (if you haven't already).

2. Ensure that your Python plugin/language server is configured to use Pigweed's
   Python virtual environment, located at
   ``$PW_PROJECT_ROOT/environment/pigweed-venv``.

3. Set up C++ code intelligence by running ``pw ide cpp --clangd-command`` to
   generate the ``clangd`` invocation for your system, and configure your
   language server to use it.

Quick Start for Setting Up Projects Using Pigweed
=================================================
If you're developing on a downstream project using Pigweed, the instructions
for upstream Pigweed may just work out of the box on your project. Give it a
try!

If it doesn't work, here are some common reasons why, and ways you can fix it
for your project.

It can't find any compilation databases
---------------------------------------
You need to generate at least one compilation database before running
``pw ide sync``. Usually you will do this with your
:ref:`build system<docs-editors-compdb>`.

It isn't working for my toolchain
---------------------------------
You may need to specify additional `query drivers <https://releases.llvm.org/10.0.0/tools/clang/tools/extra/docs/clangd/Configuration.html#query-driver>`_
to allow ``clangd`` to use your toolchains. Refer to the
:ref:`pw_ide documentation<module-pw_ide-configuration>` for information on
configuring this.

Using Visual Studio Code
========================

Code Intelligence
-----------------
If you followed the quick start steps above, you're already set! Here's a
non-exhaustive list of cool features you can now enjoy:

* C/C++

  * Code navigation, including routing through facades to the correct backend

  * Code completion, including correct class members and function signatures

  * Tool tips with docs, inferred types for ``auto``, inferred values for
    ``constexpr``, data type sizes, etc.

  * Compiler errors and warnings

* Python

  * Code navigation and code completion

  * Tool tips with docs

  * Errors, warnings, and linting feedback

Integrated Terminal
-------------------
When launching the integrated terminal, it will automatically activate the
Pigweed environment within that shell session.

Configuration
-------------
See the :ref:`pw_ide docs<module-pw_ide-vscode>`.

Code Intelligence Features for Specific Languages
=================================================

C/C++
-----
Pigweed projects have a few characteristics that make it challenging for some
IDEs and language servers to support C/C++ code intelligence out of the box:

* Pigweed projects generally don't use the default host toolchain.

* Pigweed projects usually use multiple toolchains for separate targets.

* Pigweed projects rely on the build system to define the relationship between
  :ref:`facades and backends<docs-module-structure-facades>` for each target.

We've found that the best solution is to use the
`clangd <https://clangd.llvm.org/>`_ language server or alternative language
servers that use the same
`compilation database format <https://clang.llvm.org/docs/JSONCompilationDatabase.html>`_
and comply with the language server protocol. We supplement that with Pigweed
tools to produce target-specific compilation databases that work well with
the language servers.

.. _docs-editors-compdb:

Producing a Compilation Database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If you're using GN, then ``gn gen out --export-compile-commands`` will output
a compilation database (``compile_commands.json``) in the ``out`` directory
along with all of the other GN build outputs (in other words, it produces the
same output as ``gn gen out`` but *additionally* produces the compilation
database).

If you're using CMake, you can enable the ``CMAKE_EXPORT_COMPILE_COMMANDS``
option to ensure a compilation database (``compile_commands.json``) is produced.
This can be done either in your project's ``CMakeLists.txt`` (i.e.
``set(CMAKE_EXPORT_COMPILE_COMMANDS ON)``), or by setting the flag when
invoking CMake (i.e. ``-DCMAKE_EXPORT_COMPILE_COMMANDS=ON``).

Bazel does not natively support generating compilation databases right now,
though you may find third party extensions that provide this functionality.

Processing the Compilation Database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If you examine a ``compile_commands.json`` file produced GN, you'll observe
two things:

* It contains multiple commands for each file, i.e. ``foo.cc`` will have
  compile commands for multiple ``clang`` host builds, multiple ``gcc`` host
  builds, multiple ``gcc-arm-none-eabi`` device builds, etc.

* It contains many commands that are not actually valid compile commands,
  because they involve targets that use Pigweed Python wrappers to do various
  static analyses.

Both of these confound ``clangd`` and will produce broken, unexpected, or
inconsistent results when used for code intelligence. So the
:ref:`pw_ide<module-pw_ide>` module provides CLI tools that process the "raw"
compilation database produced by the build system into one or more "clean"
compilation databases that will work smoothly with ``clangd``.

Once you have a compilation database, run this command to process it:

.. code-block:: bash

   pw ide cpp --process <path to compilation database>

Or better yet, just let ``pw_ide`` find any compilation databases you have
in your build and process them:

.. code-block:: bash

   pw ide cpp --process

If your ``compile_commands.json`` file *did not* come from GN, it may not
exhibit any of these problems and therefore not require any processing.
Nonetheless, you can still run ``pw ide cpp --process``; ``pw_ide`` will
determine if the file needs processing or not.

.. admonition:: Note
   :class: warning

   **How often do we need to process the compilation database?** With GN, if
   you're just editing existing files, there's no need to reprocess the
   compilation database. But any time the build changes (e.g. adding new source
   files or new targets), you will need to reprocess the compilation database.

   With CMake, it is usually not necessary to reprocess compilation databases,
   since ``pw_ide`` will automatically pick up any changes that CMake makes.

Setting the Target to Use for Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Discover which targets are available for code analysis:

.. code-block::

   $ pw ide cpp --list

   C/C++ targets available for language server analysis:
         pw_strict_host_gcc_debug
         pw_strict_host_clang_debug
         stm32f429i_disc1_debug

Select the target you want to use for code analysis:

.. code-block::

   $ pw ide cpp --set pw_strict_host_gcc_debug

   Set C/C++ langauge server analysis target to: pw_strict_host_gcc_debug

Check which target is currently used for code analysis:

.. code-block::

   $ pw ide cpp

   Current C/C++ language server analysis target: pw_strict_host_gcc_debug

Your target selection will remain stable even after reprocessing the compilation
database. Your editor configuration also remains stable; you don't need to
change the editor configuration to change the target you're analyzing.
