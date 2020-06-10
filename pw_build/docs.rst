.. default-domain:: cpp

.. highlight:: sh

.. _chapter-build:

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
.. code::

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

pw_python_script
^^^^^^^^^^^^^^^^
The ``pw_python_script`` template is a convenience wrapper around ``action`` for
running Python scripts. The main benefit it provides is automatic resolution of
GN paths to filesystem paths and GN target names to compiled binary files. This
allows Python scripts to be written independent of GN, taking only filesystem as
arguments.

Another convenience provided by the template is to allow running scripts without
any outputs. Sometimes scripts run in a build do not directly produce output
files, but GN requires that all actions have an output. ``pw_python_script``
solves this by accepting a boolean ``stamp`` argument which tells it to create a
dummy output file for the action.

**Arguments**

``pw_python_script`` accepts all of the arguments of a regular ``action``
target. Additionally, it has some of its own arguments:

* ``stamp``: Optional boolean indicating whether to automatically create a dummy
  output file for the script. This allows running scripts without specifying any
  ``outputs``.

**Example**

.. code::

  import("$dir_pw_build/python_script.gni")

  python_script("hello_world") {
    script = "py/say_hello.py"
    args = [ "world" ]
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

.. code::

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

CMake / Ninja
=============

Pigweed's CMake support is provided primarily for projects that have an existing
CMake build and wish to integrate Pigweed without switching to a new build
system.

The following command generates Ninja build files in the out/cmake directory.

.. code:: sh

  cmake -B out/cmake -S /path/to/pigweed -G Ninja

Tests can be executed with the ``pw_run_tests_GROUP`` targets. To run the basic
Pigweed tests, run ``ninja -C out/cmake pw_run_tests_modules``.

CMake functions
---------------
CMake convenience functions are defined in ``pw_build/pigweed.cmake``.

* ``pw_auto_add_simple_module`` -- For modules with only one library,
  automatically declare the library and its tests.
* ``pw_add_facade`` -- Declare a module facade.
* ``pw_add_module_library`` -- Add a library that is part of a module.
* ``pw_add_test`` -- Declare a test target.

See ``pw_build/pigweed.cmake`` for the complete documentation of these
functions.

Special libraries that do not fit well with these functions are created with the
standard CMake functions, such as ``add_library`` and ``target_link_libraries``.

Use Pigweed from an existing CMake project
------------------------------------------
To use Pigweed libraries form a CMake-based project, simply include the Pigweed
repository from a ``CMakeLists.txt``.

.. code:: cmake

  add_subdirectory(path/to/pigweed pigweed)

All module libraries will be available as ``module_name`` or
``module_name.sublibrary``.

If desired, modules can be included individually.

.. code:: cmake

  include(path/to/pigweed/pw_build/pigweed.cmake)

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
