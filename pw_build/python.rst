.. _module-pw_build-python:

-------------------
Python GN templates
-------------------
The Python build is implemented with GN templates defined in
``pw_build/python.gni``. That file contains the complete usage documentation.

.. seealso:: :ref:`docs-python-build`

pw_python_package
=================
The main Python template is ``pw_python_package``. Each ``pw_python_package``
target represents a Python package. As described in
:ref:`module-pw_build-python-target`, each ``pw_python_package`` expands to
several subtargets. In summary, these are:

- ``<name>`` - Represents the files themselves
- ``<name>.lint`` - Runs static analysis
- ``<name>.tests`` - Runs all tests for this package
- ``<name>.install`` - Installs the package
- ``<name>.wheel`` - Builds a Python wheel

GN permits using abbreviated labels when the target name matches the directory
name (e.g. ``//foo`` for ``//foo:foo``). For consistency with this, Python
package subtargets are aliased to the directory when the target name is the
same as the directory. For example, these two labels are equivalent:

.. code-block::

  //path/to/my_python_package:my_python_package.tests
  //path/to/my_python_package:tests

Arguments
---------
- ``setup`` - List of setup file paths (setup.py or pyproject.toml & setup.cfg),
  which must all be in the same directory.
- ``generate_setup``: As an alternative to ``setup``, generate setup files with
  the keywords in this scope. ``name`` is required. For example:

  .. code-block::

    generate_setup = {
      name = "a_nifty_package"
      version = "1.2a"
    }

- ``sources`` - Python sources files in the package.
- ``tests`` - Test files for this Python package.
- ``python_deps`` - Dependencies on other pw_python_packages in the GN build.
- ``python_test_deps`` - Test-only pw_python_package dependencies.
- ``other_deps`` - Dependencies on GN targets that are not pw_python_packages.
- ``inputs`` - Other files to track, such as package_data.
- ``proto_library`` - A pw_proto_library target to embed in this Python package.
  ``generate_setup`` is required in place of setup if proto_library is used. See
  :ref:`module-pw_protobuf_compiler-add-to-python-package`.
- ``static_analysis`` List of static analysis tools to run; ``"*"`` (default)
  runs all tools. The supported tools are ``"mypy"`` and ``"pylint"``.
- ``pylintrc`` - Optional path to a pylintrc configuration file to use. If not
  provided, Pylint's default rcfile search is used. Pylint is executed
  from the package's setup directory, so pylintrc files in that directory
  will take precedence over others.
- ``mypy_ini`` - Optional path to a mypy configuration file to use. If not
  provided, mypy's default configuration file search is used. mypy is
  executed from the package's setup directory, so mypy.ini files in that
  directory will take precedence over others.

Example
-------
This is an example Python package declaration for a ``pw_my_module`` module.

.. code-block::

  import("//build_overrides/pigweed.gni")

  import("$dir_pw_build/python.gni")

  pw_python_package("py") {
    setup = [ "setup.py" ]
    sources = [
      "pw_my_module/__init__.py",
      "pw_my_module/alfa.py",
      "pw_my_module/bravo.py",
      "pw_my_module/charlie.py",
    ]
    tests = [
      "alfa_test.py",
      "charlie_test.py",
    ]
    python_deps = [
      "$dir_pw_status/py",
      ":some_protos.python",
    ]
    python_test_deps = [ "$dir_pw_build/py" ]
    pylintrc = "$dir_pigweed/.pylintrc"
  }


.. _module-pw_build-python-wheels:

Collecting Python wheels for distribution
-----------------------------------------
The ``.wheel`` subtarget generates a wheel (``.whl``) for the Python package.
Wheels for a package and its transitive dependencies can be collected by
traversing the ``pw_python_package_wheels`` `GN metadata
<https://gn.googlesource.com/gn/+/master/docs/reference.md#var_metadata>`_ key,
which lists the output directory for each wheel.

The ``pw_mirror_tree`` template can be used to collect wheels in an output
directory:

.. code-block::

  import("$dir_pw_build/mirror_tree.gni")

  pw_mirror_tree("my_wheels") {
    path_data_keys = [ "pw_python_package_wheels" ]
    deps = [ ":python_packages.wheel" ]
    directory = "$root_out_dir/the_wheels"
  }

pw_python_script
================
A ``pw_python_script`` represents a set of standalone Python scripts and/or
tests. These files support all of the arguments of ``pw_python_package`` except
those ``setup``. These targets can be installed, but this only installs their
dependencies.

pw_python_group
===============
Represents a group of ``pw_python_package`` and ``pw_python_script`` targets.
These targets do not add any files. Their subtargets simply forward to those of
their dependencies.

pw_python_requirements
======================
Represents a set of local and PyPI requirements, with no associated source
files. These targets serve the role of a ``requirements.txt`` file.
