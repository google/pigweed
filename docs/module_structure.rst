.. default-domain:: cpp

.. highlight:: sh

.. _chapter-module-guide:

----------------
Module Structure
----------------
The Pigweed module structure is designed to keep as much code as possible for a
particular slice of functionality in one place. That means including the code
from multiple languages, as well as all the related documentation and tests.

Additionally, the structure is designed to limit the number of places a file
could go, so that when reading callsites it is obvious where a header is from.
That is where the duplicated ``<module>`` occurrences in file paths comes from.

Example module structure
------------------------
.. code-block:: python

  pw_foo/...

    docs.rst   # If there is just 1 docs file, call it docs.rst
    README.md  # All modules must have a short README for gittiles

    BUILD.gn   # GN build required
    BUILD      # Bazel build required

    # C++ public headers; the repeated module name is required
    public/pw_foo/foo.h
    public/pw_foo/baz.h

    # Exposed public headers go under internal/
    public/pw_foo/internal/bar.h
    public/pw_foo/internal/qux.h

    # Public override headers must go in 'public_overrides'
    public_overrides/gtest/gtest.h
    public_overrides/string.h

    # Private headers go into <module>_*/...
    pw_foo_internal/zap.h
    pw_foo_private/zip.h
    pw_foo_secret/alxx.h

    # C++ implementations go in the root
    foo_impl.cc
    foo.cc
    baz.cc
    bar.cc
    zap.cc
    zip.cc
    alxx.cc

    # C++ tests also go in the root
    foo_test.cc
    bar_test.cc
    zip_test.cc

    # Python files go into 'py/<module>/...'
    py/setup.py     # All Python must be a Python module with setup.py
    py/foo_test.py  # Tests go in py/ but outside of the Python module
    py/bar_test.py
    py/pw_foo/__init__.py
    py/pw_foo/__main__.py
    py/pw_foo/bar.py

    # Go files go into 'go/...'
    go/...

    # Examples go in examples/, mixing different languages
    examples/demo.py
    examples/demo.cc
    examples/demo.go
    examples/BUILD.gn
    examples/BUILD

    # Size reports go under size_report/
    size_report/BUILD.gn
    size_report/base.cc
    size_report/use_case_a.cc
    size_report/use_case_b.cc

    # Protobuf definition files go into <module>_protos/...
    pw_foo_protos/foo.proto
    pw_foo_protos/internal/zap.proto

    # Other directories are fine, but should be private.
    data/...
    graphics/...
    collection_of_tests/...
    code_relating_to_subfeature/...

Module name
-----------
Pigweed upstream modules are always named with a prefix ``pw_`` to enforce
namespacing. Projects using Pigweed that wish to make their own modules can use
whatever name they like, but we suggest picking a short prefix to namespace
your product (e.g. for an Internet of Toast project, perhaps the prefix could
be ``it_``).

C++ file and directory locations
--------------------------------

C++ public headers
~~~~~~~~~~~~~~~~~~
Located ``{pw_module_dir}/public/<module>``. These are headers that must be
exposed due to C++ limitations (i.e. are included from the public interface,
but are not intended for public use).

**Public headers** should take the form:

``{pw_module_dir}/public/<module>/*.h``

**Exposed private headers** should take the form:

``{pw_module_dir}/public/<module>/internal/*.h``

Examples:

.. code-block::

  pw_foo/...
    public/pw_foo/foo.h
    public/pw_foo/a_header.h
    public/pw_foo/baz.h

For headers that must be exposed due to C++ limitations (i.e. are included from
the public interface, but are not intended for use), place the headers in a
``internal`` subfolder under the public headers directory; as
``{pw_module_dir}/public/<module>/internal/*.h``. For example:

.. code-block::

  pw_foo/...
    public/pw_foo/internal/secret.h
    public/pw_foo/internal/business.h

.. note::

  These headers must not override headers from other modules. For
  that, there is the ``public_overrides/`` directory.

Public override headers
~~~~~~~~~~~~~~~~~~~~~~~
Located ``{pw_module_dir}/public_overrides/<module>``. In general, the Pigweed
philosophy is to avoid having "things hiding under rocks", and having header
files with the same name that can override each other is considered a rock
where surprising things can hide. Additionally, a design goal of the Pigweed
module structure is to make it so there is ideally exactly one obvious place
to find a header based on an ``#include``.

However, in some cases header overrides are necessary to enable flexibly
combining modules. To make this as explicit as possible, headers which override
other headers must go in

``{pw_module_dir}/public_overrides/...```

For example, the ``pw_unit_test`` module provides a header override for
``gtest/gtest.h``. The structure of the module is (omitting some files):

.. code-block::

  pw_unit_test/...

    public_overrides/gtest
    public_overrides/gtest/gtest.h

    public/pw_unit_test
    public/pw_unit_test/framework.h
    public/pw_unit_test/simple_printing_event_handler.h
    public/pw_unit_test/event_handler.h

Note that the overrides are in a separate directory ``public_overrides``.

C++ implementation files
~~~~~~~~~~~~~~~~~~~~~~~~
Located ``{pw_module_dir}/``. C++ implementation files go at the top level of
the module. Implementation files must always use "" style includes.

Example:

.. code-block::

  pw_unit_test/...
    main.cc
    framework.cc
    test.gni
    BUILD.gn
    README.md

Documentation
~~~~~~~~~~~~~
Documentation should go in the root module folder, typically in the
``docs.rst`` file. There must be a docgen entry for the documentation in the
``BUILD.gn`` file with the target name ``docs``; so the full target for the
docs would be ``<module>:docs``.

.. code-block::

  pw_example_module/...

    docs.rst

For modules with more involved documentation, create a separate directory
called ``docs/`` under the module root, and put the ``.rst`` files and other
related files (like images and diagrams) there.

.. code-block::

  pw_example_module/...

    docs/docs.rst
    docs/bar.rst
    docs/foo.rst
    docs/image/screenshot.png
    docs/image/diagram.svg

Steps to create a new module for a Pigweed project
--------------------------------------------------
These instructions are for creating a new module for contribution to the
Pigweed project. See below for an `example`__ of what the new module folder
might look like.

__ `Example module structure`_

1. Create module folder following `Module name`_ guidelines
2. Add `C++ public headers`_ files in
   ``{pw_module_dir}/public/{pw_module_name}/``
3. Add `C++ implementation files`_ files in ``{pw_module_dir}/``
4. Add module documentation

    - Add ``{pw_module_dir}/README.md`` that has a module summary
    - Add ``{pw_module_dir}/docs.rst`` that contains the main module
      documentation

5. Add build support inside of new module

    - Add GN with ``{pw_module_dir}/BUILD.gn``
    - Add Bazel with ``{pw_module_dir}/BUILD``
    - Add CMake with ``{pw_module_dir}/CMakeLists.txt``

6. Add folder alias for new module variable in ``/modules.gni``

    - dir_pw_new = "$dir_pigweed/pw_new"

7. Add new module to main GN build

    - in ``/BUILD.gn`` to ``group("pw_modules")`` using folder alias variable

8. Add test target for new module in ``/BUILD.gn`` to
   ``pw_test_group("pw_module_tests")``
9. Add new module to CMake build

    - In ``/CMakeLists.txt`` add ``add_subdirectory(pw_new)``

10. Add the new module to docs module

    - Add in ``docs/BUILD.gn`` to ``pw_doc_gen("docs")``

11. Run :ref:`chapter-module-module-check`

    - ``$ pw module-check {pw_module_dir}``

