.. _docs-python-build:

======================
Pigweed's Python build
======================
Pigweed uses a custom GN-based build system to manage its Python code. The
Pigweed Python build supports packaging, installation, and distribution of
interdependent local Python packages. It also provides for fast, incremental
static analysis and test running suitable for live use during development (e.g.
with :ref:`module-pw_watch`) or in continuous integration.

Pigweed's Python code is exclusively managed by GN, but the GN-based build may
be used alongside CMake, Bazel, or any other build system. Pigweed's environment
setup uses GN to set up the initial Python environment, regardless of the final
build system. As needed, non-GN projects can declare just their Python packages
in GN.

Background
==========
Developing software involves much more than writing source code. Software needs
to be compiled, executed, tested, analyzed, packaged, and deployed. As projects
grow beyond a few files, these tasks become impractical to manage manually.
Build systems automate these auxiliary tasks of software development, making it
possible to build larger, more complex systems quickly and robustly.

Python is an interpreted language, but it shares most build automation concerns
with other languages. Pigweed uses Python extensively and must to address these
needs for itself and its users.

Existing solutions
==================
The Python programming langauge does not have an official build automation
system. However, there are numerous Python-focused build automation tools with
varying degrees of adoption. See the `Python Wiki
<https://wiki.python.org/moin/ConfigurationAndBuildTools>`_ for examples.

A few Python tools have become defacto standards, including `setuptools
<https://pypi.org/project/setuptools/>`_, `wheel
<https://pypi.org/project/wheel/>`_, and `pip <https://pypi.org/project/pip/>`_.
These essential tools address key aspects of Python packaging and distribution,
but are not intended for general build automation. Tools like `PyBuilder
<https://pybuilder.io/>`_ and `tox <https://tox.readthedocs.io/en/latest/>`_
provide more general build automation for Python.

The `Bazel <http://bazel.build/>`_ build system has first class support for
Python and other languages used by Pigweed, including protocol buffers.

Challenges
==========
Pigweed's use of Python is different from many other projects. Pigweed is a
multi-language, modular project. It serves both as a library or middleware and
as a development environment.

This section describes Python build automation challenges encountered by
Pigweed.

Dependencies
------------
Pigweed is organized into distinct modules. In Python, each module is a separate
package, potentially with dependencies on other local or `PyPI
<https://pypi.org/>`_ packages.

The basic Python packaging tools lack dependency tracking for local packages.
For example, a package's ``setup.py`` or ``setup.cfg`` lists all of
its dependencies, but ``pip`` is not aware of local packages until they are
installed. Packages must be installed with their dependencies taken into
account, in topological sorted order.

To work around this, one could set up a private `PyPI server
<https://pypi.org/project/pypiserver/>`_ instance, but this is too cumbersome
for daily development and incompatible with editable package installation.

Testing
-------
Tests are crucial to having a healthy, maintainable codebase. While they take
some initial work to write, the time investment pays for itself many times over
by contributing to the long-term resilience of a codebase. Despite their
benefit, developers don't always take the time to write tests. Any barriers to
writing and running tests result in fewer tests and consequently more fragile,
bug-prone codebases.

There are lots of great Python libraries for testing, such as
`unittest <https://docs.python.org/3/library/unittest.html>`_ and
`pytest <https://docs.pytest.org/en/stable/>`_. These tools make it easy to
write and execute individual Python tests, but are not well suited for managing
suites of interdependent tests in a large project. Writing a test with these
utilities does not automatically run them or keep running them as the codebase
changes.

Static analysis
---------------
Various static analysis tools exist for Python. Two widely used, powerful tools
are `Pylint <https://www.pylint.org/>`_ and `Mypy <http://mypy-lang.org/>`_.
Using these tools improves code quality, as they catch bugs, encourage good
design practices, and enforce a consistent coding style. As with testing,
barriers to running static analysis tools cause many developers to skip them.
Some developers may not even be aware of these tools.

Deploying static analysis tools to a codebase like Pigweed has some challenges.
Mypy and Pylint are simple to run, but they are extremely slow. Ideally, these
tools would be run constantly during development, but only on files that change.
These tools do not have built-in support for incremental runs or dependency
tracking.

Another challenge is configuration. Mypy and Pylint support using configuration
files to select which checks to run and how to apply them. Both tools only
support using a single configuration file for an entire run, which poses a
challenge to modular middleware systems where different parts of a project may
require different configurations.

Protocol buffers
----------------
`Protocol buffers <https://developers.google.com/protocol-buffers>`_ are an
efficient system for serializing structured data. They are widely used by Google
and other companies.

The protobuf compiler ``protoc`` generates Python modules from ``.proto`` files.
``protoc`` strictly generates protobuf modules according to their directory
structure. This works well in a monorepo, but poses a challenge to a middleware
system like Pigweed. Generating protobufs by path also makes integrating
protobufs with existing packages awkward.

Requirements
============
Pigweed aims to provide high quality software components and a fast, effective,
flexible development experience for its customers. Pigweed's high-level goals
and the `challenges`_ described above inform these requirements for the Pigweed
Python build.

- Integrate seamlessly with the other Pigweed build tools.
- Easy to use independently, even if primarily using a different build system.
- Support standard packaging and distribution with setuptools, wheel, and pip.
- Correctly manage interdependent local Python packages.
- Out-of-the-box support for writing and running tests.
- Preconfigured, trivial-to-run static analysis integration for Pylint and Mypy.
- Fast, dependency-aware incremental rebuilds and test execution, suitable for
  use with :ref:`module-pw_watch`.
- Seamless protocol buffer support.

Detailed design
===============

Build automation tool
---------------------
Existing Python tools may be effective for Python codebases, but their utility
is more limited in a multi-language project like Pigweed. The cost of bringing
up and maintaining an additional build automation system for a single language
is high.

Pigweed uses GN as its primary build system for all languages. While GN does not
natively support Python, adding support is straightforward with GN templates.

GN has strong multi-toolchain and multi-language capabilities. In GN, it is
straightforward to share targets and artifacts between different languages. For
example, C++, Go, and Python targets can depend on the same protobuf
declaration. When using GN for multiple languages, Ninja schedules build steps
for all languages together, resulting in faster total build times.

Not all Pigweed users build with GN. Of Pigweed's three supported build systems,
GN is the fastest, lightest weight, and easiest to run. It also has simple,
clean syntax. This makes it feasible to use GN only for Python while building
primarily with a different system.

Given these considerations, GN is an ideal choice for Pigweed's Python build.

.. _docs-python-build-structure:

Module structure
----------------
Pigweed Python code is structured into standard Python packages. This makes it
simple to package and distribute Pigweed Python packages with common Python
tools.

Like all Pigweed source code, Python packages are organized into Pigweed
modules. A module's Python package is nested under a ``py/`` directory (see
:ref:`docs-module-structure`).

.. code-block::
  :caption: Example layout of a Pigweed Python package.
  :name: python-file-tree

  module_name/
  ├── py/
  │   ├── BUILD.gn
  │   ├── setup.cfg
  │   ├── setup.py
  │   ├── pyproject.toml
  │   ├── package_name/
  │   │   ├── module_a.py
  │   │   ├── module_b.py
  │   │   ├── py.typed
  │   │   └── nested_package/
  │   │       ├── py.typed
  │   │       └── module_c.py
  │   ├── module_a_test.py
  │   └── module_c_test.py
  └── ...

The ``BUILD.gn`` declares this package in GN. For upstream Pigweed, a presubmit
check in ensures that all Python files are listed in a ``BUILD.gn``.

Pigweed prefers to define Python packages using ``setup.cfg`` files. In the
above file tree ``setup.py`` and ``pyproject.toml`` files are stubs with the
following content:

.. code-block::
  :caption: setup.py
  :name: setup-py-stub

  import setuptools  # type: ignore
  setuptools.setup()  # Package definition in setup.cfg

.. code-block::
  :caption: pyproject.toml
  :name: pyproject-toml-stub

  [build-system]
  requires = ['setuptools', 'wheel']
  build-backend = 'setuptools.build_meta'

The stub ``setup.py`` file is there to support running ``pip install --editable``.

Each ``pyproject.toml`` file is required to specify which build system should be
used for the given Python package. In Pigweed's case it always specifies using
setuptools.

.. seealso::

   - ``setup.cfg`` examples at `Configuring setup() using setup.cfg files`_
   - ``pyproject.toml`` background at `Build System Support - How to use it?`_


.. _module-pw_build-python-target:

pw_python_package targets
-------------------------
The key abstraction in the Python build is the ``pw_python_package``.
A ``pw_python_package`` represents a Python package as a GN target. It is
implemented with a GN template. The ``pw_python_package`` template is documented
in :ref:`module-pw_build-python`.

The key attributes of a ``pw_python_package`` are

- a ``setup.py`` file,
- source files,
- test files,
- dependencies on other ``pw_python_package`` targets.

A ``pw_python_package`` target is composed of several GN subtargets. Each
subtarget represents different functionality in the Python build.

- ``<name>`` - Represents the Python files in the build, but does not take any
  actions. All subtargets depend on this target.
- ``<name>.tests`` - Runs all tests for this package.

  - ``<name>.tests.<test_file>`` - Runs the specified test.

- ``<name>.lint`` - Runs static analysis tools on the Python code. This is a
  group of two subtargets:

  - ``<name>.lint.mypy`` - Runs Mypy on all Python files, if enabled.
  - ``<name>.lint.pylint`` - Runs Pylint on all Python files, if enabled.

- ``<name>.install`` - Installs the package in a Python virtual environment.
- ``<name>.wheel`` - Builds a Python wheel for this package.

To avoid unnecessary duplication, all Python actions are executed in the default
toolchain, even if they are referred to from other toolchains.

Testing
^^^^^^^
Tests for a Python package are listed in its ``pw_python_package`` target.
Adding a new test is simple: write the test file and list it in its accompanying
Python package. The build will run the it when the test, the package, or one
of its dependencies is updated.

Static analysis
^^^^^^^^^^^^^^^
``pw_python_package`` targets are preconfigured to run Pylint and Mypy on their
source and test files. Users may specify which  ``pylintrc`` and ``mypy.ini``
files to
use on a per-package basis. The configuration files may also be provided in the
directory structure; the tools will locate them using their standard means. Like
tests, static analysis is only run when files or their dependencies change.

Packages may opt out of static analysis as necessary.

Installing packages in a virtual environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Python packages declared in the Python build may be installed in a specified
`virtual environment <https://docs.python.org/3/tutorial/venv.html>`_. The
default venv to use may be specified using a GN build arg. The venv may be
overridden for individual targets. The Python build tracks installation status
of packages based on which venv is in use.

The Python build examines the ``VIRTUAL_ENV`` environment variable to determine
the current venv. If the selected virtual environment is active, packages are
installed directly into it. If the venv is not active, it is activated before
installing the packages.

.. admonition:: Under construction

  Pigweed Python targets are not yet aware of the virtual environment.
  Currently, all actions occur in the active venv without consulting
  ``VIRTUAL_ENV``.

Python packages defined entirely in tree are installed with the ``--editable``
option. Partially or fully generated packages are installed without that option.

Building Python wheels
^^^^^^^^^^^^^^^^^^^^^^
Wheels are the standard format for distributing Python packages. The Pigweed
Python build supports creating wheels for individual packages and groups of
packages. Building the ``.wheel`` subtarget creates a ``.whl`` file for the
package using the PyPA's `build <https://pypa-build.readthedocs.io/en/stable/>`_
tool.

The ``.wheel`` subtarget of ``pw_python_package`` records the location of
the generated wheel with `GN metadata
<https://gn.googlesource.com/gn/+/HEAD/docs/reference.md#var_metadata>`_.
Wheels for a Python package and its transitive dependencies can be collected
from the ``pw_python_package_wheels`` key. See
:ref:`module-pw_build-python-dist`.

Protocol buffers
^^^^^^^^^^^^^^^^
The Pigweed GN build supports protocol buffers with the ``pw_proto_library``
target (see :ref:`module-pw_protobuf_compiler`). Python protobuf modules are
generated as standalone Python packages by default. Protocol buffers may also be
nested within existing Python packages. In this case, the Python package in the
source tree is incomplete; the final Python package, including protobufs, is
generated in the output directory.

Generating setup.py
-------------------
The ``pw_python_package`` target in the ``BUILD.gn`` duplicates much of the
information in the ``setup.py`` or ``setup.cfg`` file. In many cases, it would
be possible to generate a ``setup.py`` file rather than including it in the
source tree. However, removing the ``setup.py`` would preclude using a direct,
editable installation from the source tree.

Pigweed packages containing protobufs are generated in full or in part. These
packages may use generated setup files, since they are always be packaged or
installed from the build output directory.

See also
========

  - :ref:`module-pw_build-python`
  - :ref:`module-pw_build`
  - :ref:`docs-build-system`

.. _Configuring setup() using setup.cfg files: https://ipython.readthedocs.io/en/stable/interactive/reference.html#embedding
.. _Build System Support - How to use it?: https://setuptools.readthedocs.io/en/latest/build_meta.html?highlight=pyproject.toml#how-to-use-it
