.. _module-pw_build:

--------
pw_build
--------
Pigweed's modules aim to be easily integratable into both new and existing
embedded projects. To that goal, the ``pw_build`` module provides support for
multiple build systems. Our personal favorite is `GN`_/`Ninja`_, which is used
by upstream developers for its speed and flexibility. `CMake`_ and `Bazel`_
build files are also provided by all modules, allowing Pigweed to be added to a
project with minimal effort.

.. _GN: https://gn.googlesource.com/gn/
.. _Ninja: https://ninja-build.org/
.. _CMake: https://cmake.org/
.. _Bazel: https://bazel.build/

Beyond just compiling code, Pigweed’s GN build system can also:

* Generate HTML documentation, via our Sphinx integration (with ``pw_docgen``)
* Display memory usage report cards (with ``pw_bloat``)
* Incrementally run unit tests after code changes (with ``pw_target_runner``)
* And more!

These are only supported in the GN build, so we recommend using it if possible.

GN / Ninja
==========
The GN / Ninja build system is the primary build system used for upstream
Pigweed development, and is the most tested and feature-rich build system
Pigweed offers.

This module's ``build.gn`` file contains a number of C/C++ ``config``
declarations that are used by upstream Pigweed to set some architecture-agnostic
compiler defaults. (See Pigweed's ``//BUILDCONFIG.gn``)

``pw_build`` also provides several useful GN templates that are used throughout
Pigweed.

Templates
---------

Target types
^^^^^^^^^^^^
.. code-block::

  import("$dir_pw_build/target_types.gni")

  pw_source_set("my_library") {
    sources = [ "lib.cc" ]
  }

Pigweed defines wrappers around the four basic GN binary types ``source_set``,
``executable``, ``static_library``, and ``shared_library``. These wrappers apply
default arguments to each target as specified in the ``default_configs`` and
``default_public_deps`` build args. Additionally, they allow defaults to be
removed on a per-target basis using ``remove_configs`` and
``remove_public_deps`` variables, respectively.

The ``pw_executable`` template provides additional functionality around building
complete binaries. As Pigweed is a collection of libraries, it does not know how
its final targets are built. ``pw_executable`` solves this by letting each user
of Pigweed specify a global executable template for their target, and have
Pigweed build against it. This is controlled by the build variable
``pw_executable_config.target_type``, specifying the name of the executable
template for a project.

.. tip::

  Prefer to use ``pw_executable`` over plain ``executable`` targets to allow
  cleanly building the same code for multiple target configs.

**Arguments**

All of the ``pw_*`` target type overrides accept any arguments, as they simply
forward them through to the underlying target.

.. _module-pw_build-facade:

pw_facade
^^^^^^^^^
In their simplest form, a :ref:`facade<docs-module-structure-facades>` is a GN
build arg used to change a dependency at compile time. Pigweed targets configure
these facades as needed.

The ``pw_facade`` template bundles a ``pw_source_set`` with a facade build arg.
This allows the facade to provide header files, compilation options or anything
else a GN ``source_set`` provides.

The ``pw_facade`` template declares two targets:

* ``$target_name``: the public-facing ``pw_source_set``, with a ``public_dep``
  on the backend
* ``$target_name.facade``: target used by the backend to avoid circular
  dependencies

.. code-block::

  # Declares ":foo" and ":foo.facade" GN targets
  pw_facade("foo") {
    backend = pw_log_BACKEND
    public_configs = [ ":public_include_path" ]
    public = [ "public/pw_foo/foo.h" ]
  }

.. _module-pw_build-python-action:

pw_python_action
^^^^^^^^^^^^^^^^
The ``pw_python_action`` template is a convenience wrapper around ``action`` for
running Python scripts. The main benefit it provides is resolution of GN target
labels to compiled binary files. This allows Python scripts to be written
independently of GN, taking only filesystem paths as arguments.

Another convenience provided by the template is to allow running scripts without
any outputs. Sometimes scripts run in a build do not directly produce output
files, but GN requires that all actions have an output. ``pw_python_action``
solves this by accepting a boolean ``stamp`` argument which tells it to create a
dummy output file for the action.

**Arguments**

``pw_python_action`` accepts all of the arguments of a regular ``action``
target. Additionally, it has some of its own arguments:

* ``module``: Run the specified Python module instead of a script. Either
  ``script`` or ``module`` must be specified, but not both.
* ``capture_output``: Optional boolean. If true, script output is hidden unless
  the script fails with an error. Defaults to true.
* ``stamp``: Optional variable indicating whether to automatically create a
  dummy output file for the script. This allows running scripts without
  specifying ``outputs``. If ``stamp`` is true, a generic output file is
  used. If ``stamp`` is a file path, that file is used as a stamp file. Like any
  output file, ``stamp`` must be in the build directory. Defaults to false.
* ``directory``: Optional path. Change to this directory before executing the
  command. Paths in arguments may need to be adjusted.
* ``environment``: Optional list of strings. Environment variables to set,
  passed as NAME=VALUE strings.

**Expressions**

``pw_python_action`` evaluates expressions in ``args``, the arguments passed to
the script. These expressions function similarly to generator expressions in
CMake. Expressions may be passed as a standalone argument or as part of another
argument. A single argument may contain multiple expressions.

Generally, these expressions are used within templates rather than directly in
BUILD.gn files. This allows build code to use GN labels without having to worry
about converting them to files.

The following expressions are supported:

.. describe:: <TARGET_FILE(gn_target)>

  Evaluates to the output file of the provided GN target. For example, the
  expression

  .. code-block::

    "<TARGET_FILE(//foo/bar:static_lib)>"

  might expand to

  .. code-block::

    "/home/User/project_root/out/obj/foo/bar/static_lib.a"

  ``TARGET_FILE`` parses the ``.ninja`` file for the GN target, so it should
  always find the correct output file, regardless of the toolchain's or target's
  configuration. Some targets, such as ``source_set`` and ``group`` targets, do
  not have an output file, and attempting to use ``TARGET_FILE`` with them
  results in an error.

  ``TARGET_FILE`` only resolves GN target labels to their outputs. To resolve
  paths generally, use the standard GN approach of applying the
  ``rebase_path(path)`` function. With default arguments, ``rebase_path``
  converts the provided GN path or list of paths to be relative to the build
  directory, from which all build commands and scripts are executed.

.. describe:: <TARGET_FILE_IF_EXISTS(gn_target)>

  ``TARGET_FILE_IF_EXISTS`` evaluates to the output file of the provided GN
  target, if the output file exists. If the output file does not exist, the
  entire argument that includes this expression is omitted, even if there is
  other text or another expression.

  For example, consider this expression:

  .. code-block::

    "--database=<TARGET_FILE_IF_EXISTS(//alpha/bravo)>"

  If the ``//alpha/bravo`` target file exists, this might expand to the
  following:

  .. code-block::

    "--database=/home/User/project/out/obj/alpha/bravo/bravo.elf"

  If the ``//alpha/bravo`` target file does not exist, the entire
  ``--database=`` argument is omitted from the script arguments.

.. describe:: <TARGET_OBJECTS(gn_target)>

  Evaluates to the object files of the provided GN target. Expands to a separate
  argument for each object file. If the target has no object files, the argument
  is omitted entirely. Because it does not expand to a single expression, the
  ``<TARGET_OBJECTS(...)>`` expression may not have leading or trailing text.

  For example, the expression

  .. code-block::

    "<TARGET_OBJECTS(//foo/bar:a_source_set)>"

  might expand to multiple separate arguments:

  .. code-block::

    "/home/User/project_root/out/obj/foo/bar/a_source_set.file_a.cc.o"
    "/home/User/project_root/out/obj/foo/bar/a_source_set.file_b.cc.o"
    "/home/User/project_root/out/obj/foo/bar/a_source_set.file_c.cc.o"

**Example**

.. code-block::

  import("$dir_pw_build/python_action.gni")

  pw_python_action("postprocess_main_image") {
    script = "py/postprocess_binary.py"
    args = [
      "--database",
      rebase_path("my/database.csv"),
      "--binary=<TARGET_FILE(//firmware/images:main)>",
    ]
    stamp = true
  }

pw_input_group
^^^^^^^^^^^^^^
``pw_input_group`` defines a group of input files which are not directly
processed by the build but are still important dependencies of later build
steps. This is commonly used alongside metadata to propagate file dependencies
through the build graph and force rebuilds on file modifications.

For example ``pw_docgen`` defines a ``pw_doc_group`` template which outputs
metadata from a list of input files. The metadata file is not actually part of
the build, and so changes to any of the input files do not trigger a rebuild.
This is problematic, as targets that depend on the metadata should rebuild when
the inputs are modified but GN cannot express this dependency.

``pw_input_group`` solves this problem by allowing a list of files to be listed
in a target that does not output any build artifacts, causing all dependent
targets to correctly rebuild.

**Arguments**

``pw_input_group`` accepts all arguments that can be passed to a ``group``
target, as well as requiring one extra:

* ``inputs``: List of input files.

**Example**

.. code-block::

  import("$dir_pw_build/input_group.gni")

  pw_input_group("foo_metadata") {
    metadata = {
      files = [
        "x.foo",
        "y.foo",
        "z.foo",
      ]
    }
    inputs = metadata.files
  }

Targets depending on ``foo_metadata`` will rebuild when any of the ``.foo``
files are modified.

pw_zip
^^^^^^
``pw_zip`` is a target that allows users to zip up a set of input files and
directories into a single output ``.zip`` file—a simple automation of a
potentially repetitive task.

**Arguments**

* ``inputs``: List of source files as well as the desired relative zip
  destination. See below for the input syntax.
* ``dirs``: List of entire directories to be zipped as well as the desired
  relative zip destination. See below for the input syntax.
* ``output``: Filename of output ``.zip`` file.
* ``deps``: List of dependencies for the target.

**Input Syntax**

Inputs all need to follow the correct syntax:

#. Path to source file or directory. Directories must end with a ``/``.
#. The delimiter (defaults to ``>``).
#. The desired destination of the contents within the ``.zip``. Must start
   with ``/`` to indicate the zip root. Any number of subdirectories are
   allowed. If the source is a file it can be put into any subdirectory of the
   root. If the source is a file, the zip copy can also be renamed by ending
   the zip destination with a filename (no trailing ``/``).

Thus, it should look like the following: ``"[source file or dir] > /"``.

**Example**

Let's say we have the following structure for a ``//source/`` directory:

.. code-block::

  source/
  ├── file1.txt
  ├── file2.txt
  ├── file3.txt
  └── some_dir/
      ├── file4.txt
      └── some_other_dir/
          └── file5.txt

And we create the following build target:

.. code-block::

  import("$dir_pw_build/zip.gni")

  pw_zip("target_name") {
    inputs = [
      "//source/file1.txt > /",             # Copied to the zip root dir.
      "//source/file2.txt > /renamed.txt",  # File renamed.
      "//source/file3.txt > /bar/",         # File moved to the /bar/ dir.
    ]

    dirs = [
      "//source/some_dir/ > /bar/some_dir/",  # All /some_dir/ contents copied
                                              # as /bar/some_dir/.
    ]

    # Note on output: if the specific output directory isn't defined
    # (such as output = "zoo.zip") then the .zip will output to the
    # same directory as the BUILD.gn file that called the target.
    output = "//$target_out_dir/foo.zip"  # Where the foo.zip will end up
  }

This will result in a ``.zip`` file called ``foo.zip`` stored in
``//$target_out_dir`` with the following structure:

.. code-block::

  foo.zip
  ├── bar/
  │   ├── file3.txt
  │   └── some_dir/
  │       ├── file4.txt
  │       └── some_other_dir/
  │           └── file5.txt
  ├── file1.txt
  └── renamed.txt

CMake / Ninja
=============
Pigweed's `CMake`_ support is provided primarily for projects that have an
existing CMake build and wish to integrate Pigweed without switching to a new
build system.

The following command generates Ninja build files for a host build in the
``out/cmake_host`` directory:

.. code-block:: sh

  cmake -B out/cmake_host -S "$PW_ROOT" -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PW_ROOT/pw_toolchain/host_clang/toolchain.cmake

The ``PW_ROOT`` environment variable must point to the root of the Pigweed
directory. This variable is set by Pigweed's environment setup.

Tests can be executed with the ``pw_run_tests.GROUP`` targets. To run Pigweed
module tests, execute ``pw_run_tests.modules``:

.. code-block:: sh

  ninja -C out/cmake_host pw_run_tests.modules

:ref:`module-pw_watch` supports CMake, so you can also run

.. code-block:: sh

  pw watch -C out/cmake_host pw_run_tests.modules

CMake functions
---------------
CMake convenience functions are defined in ``pw_build/pigweed.cmake``.

* ``pw_auto_add_simple_module`` -- For modules with only one library,
  automatically declare the library and its tests.
* ``pw_auto_add_module_tests`` -- Create test targets for all tests in a module.
* ``pw_add_facade`` -- Declare a module facade.
* ``pw_set_backend`` -- Set the backend library to use for a facade.
* ``pw_add_module_library`` -- Add a library that is part of a module.
* ``pw_add_test`` -- Declare a test target.

See ``pw_build/pigweed.cmake`` for the complete documentation of these
functions.

Special libraries that do not fit well with these functions are created with the
standard CMake functions, such as ``add_library`` and ``target_link_libraries``.

Facades and backends
--------------------
The CMake build uses CMake cache variables for configuring
:ref:`facades<docs-module-structure-facades>` and backends. Cache variables are
similar to GN's build args set with ``gn args``. Unlike GN, CMake does not
support multi-toolchain builds, so these variables have a single global value
per build directory.

The ``pw_add_facade`` function declares a cache variable named
``<module_name>_BACKEND`` for each facade. Cache variables can be awkward to
work with, since their values only change when they're assigned, but then
persist accross CMake invocations. These variables should be set in one of the
following ways:

* Call ``pw_set_backend`` to set backends appropriate for the target in the
  target's toolchain file. The toolchain file is provided to ``cmake`` with
  ``-DCMAKE_TOOLCHAIN_FILE=<toolchain file>``.
* Call ``pw_set_backend`` in the top-level ``CMakeLists.txt`` before other
  CMake code executes.
* Set the backend variable at the command line with the ``-D`` option.

  .. code-block:: sh

    cmake -B out/cmake_host -S "$PW_ROOT" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$PW_ROOT/pw_toolchain/host_clang/toolchain.cmake \
        -Dpw_log_BACKEND=pw_log_basic

* Temporarily override a backend by setting it interactively with ``ccmake`` or
  ``cmake-gui``.

Toolchain setup
---------------
In CMake, the toolchain is configured by setting CMake variables, as described
in the `CMake documentation <https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html>`_.
These variables are typically set in a toolchain CMake file passed to ``cmake``
with the ``-D`` option (``-DCMAKE_TOOLCHAIN_FILE=path/to/file.cmake``).
For Pigweed embedded builds, set ``CMAKE_SYSTEM_NAME`` to the empty string
(``""``).

Third party libraries
---------------------
The CMake build includes third-party libraries similarly to the GN build. A
``dir_pw_third_party_<library>`` cache variable is defined for each third-party
dependency. This variable can have one of three values:

* ``""`` (empty) -- the dependency is not available
* ``PRESENT`` -- the dependency is available and is already included in the
  build
* ``</path/to/the/dependency>`` -- the dependency is available and will be
  automatically imported from this path using ``add_subdirectory``.

If the variable is empty (``if("${dir_pw_third_party_<library>}" STREQUAL
"")``), the dependency is not available. Otherwise, it is available and
libraries declared by it can be referenced.

Third party variables are set like any other cache global variable in CMake. It
is recommended to set these in one of the following ways:

* Set with the CMake ``set`` function in the toolchain file or a
  ``CMakeLists.txt`` before other CMake code executes.

  .. code-block:: cmake

    set(dir_pw_third_party_nanopb PRESENT CACHE STRING "" FORCE)

* Set the variable at the command line with the ``-D`` option.

  .. code-block:: sh

    cmake -B out/cmake_host -S "$PW_ROOT" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$PW_ROOT/pw_toolchain/host_clang/toolchain.cmake \
        -Ddir_pw_third_party_nanopb=/path/to/nanopb

* Set the variable interactively with ``ccmake`` or ``cmake-gui``.

Use Pigweed from an existing CMake project
------------------------------------------
To use Pigweed libraries form a CMake-based project, simply include the Pigweed
repository from a ``CMakeLists.txt``.

.. code-block:: cmake

  add_subdirectory(path/to/pigweed pigweed)

All module libraries will be available as ``module_name`` or
``module_name.sublibrary``.

If desired, modules can be included individually.

.. code-block:: cmake

  add_subdirectory(path/to/pigweed/pw_some_module pw_some_module)
  add_subdirectory(path/to/pigweed/pw_another_module pw_another_module)

Bazel
=====
Bazel is currently very experimental, and only builds for host.

The common configuration for Bazel for all modules is in the ``pigweed.bzl``
file. The built-in Bazel rules ``cc_binary``, ``cc_library``, and ``cc_test``
are wrapped with ``pw_cc_binary``, ``pw_cc_library``, and ``pw_cc_test``.
These wrappers add parameters to calls to the compiler and linker.

The ``BUILD`` file is merely a placeholder and currently does nothing.
