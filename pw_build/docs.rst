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

The module also provides some useful GN templates for build targets.

Templates
---------

pw_executable
^^^^^^^^^^^^^
.. code::

  import("$dir_pw_build/pw_executable.gni")

The ``pw_executable`` template is a wrapper around executable targets which
builds for a globally-defined target type. This is controlled by the build
variable ``pw_executable_config.target_type``. For example, setting this
variable to ``stm32f429i_executable`` causes all executable targets to build
using that template.

.. tip::

  Prefer to use ``pw_executable`` over plain ``executable`` targets to allow
  cleanly building the same code for multiple target configs.

**Arguments**

``pw_executable`` accepts any arguments, as it simply forwards them through to
the specified custom template.

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

Bazel
=====
The common configuration for Bazel for all modules is in the ``pigweed.bzl``
file. The built-in Bazel rules ``cc_binary``, ``cc_library``, and ``cc_test``
are wrapped with ``pw_cc_binary``, ``pw_cc_library``, and ``pw_cc_test``.
These wrappers add parameters to calls to the compiler and linker.

The ``BUILD`` file is merely a placeholder and currently does nothing.
