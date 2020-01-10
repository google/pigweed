.. _chapter-module:

.. default-domain:: cpp

.. highlight:: sh

---------
pw_module
---------

The ``pw_module`` module is the "meta-module" for Pigweed, containing
documentation for Pigweed's module structure as well as tools to detect when
modules are in or out of conformance to the module style.

The Pigweed module structure
----------------------------

The Pigweed module structure is designed to keep as much code as possible for a
particular slice of functionality in one place. That means including the code
from multiple languages, as well as all the related documentation and tests.

Additionally, the structure is designed to limit the number of places a file
could go, so that when reading callsites it is obvious where a header is from.
That is where the duplicated ``<module>`` occurrences in file paths comes from.

tl;dr example module structure
------------------------------

.. code-block:: python

  pw_foo/...

    docs.rst   # If there is just 1 docs file, call it docs.rst
    README.md  # All modules must have a short README for gittiles

    BUILD.gn   # GN build required
    BUILD      # Bazel build required

    # Public headers; the repeated module name is required
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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Public headers - ``public/<module>``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
These are headers that must be exposed due to C++ limitations (i.e. are
included from the public interface, but are not intended for public use).

**Public headers** should take the form:

``<module>/public/<module>/*.h``

**Exposed private headers** should take the form:

``<module>/public/<module>/internal/*.h``

Examples:

.. code-block::

  pw_foo/...
    public/pw_foo/foo.h
    public/pw_foo/a_header.h
    public/pw_foo/baz.h

For headers that must be exposed due to C++ limitations (i.e. are included from
the public interface, but are not intended for use), place the headers in a
``internal`` subfolder under the public headers directory; as
``public/<module>/internal/*.h``. For example:

.. code-block::

  pw_foo/...
    public/pw_foo/internal/secret.h
    public/pw_foo/internal/business.h

.. note::

  These headers must not override headers from other modules. For
  that, there is the ``public_overrides/`` directory.

Public override headers - ``public_overrides/<module>``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In general, the Pigweed philosophy is to avoid having "things hiding under
rocks", and having header files with the same name that can override each other
is considered a rock where surprising things can hide. Additionally, a design
goal of the Pigweed module structure is to make it so there is ideally exactly
one obvious place to find a header based on an ``#include``.

However, in some cases header overrides are necessary to enable flexibly
combining modules. To make this as explicit as possible, headers which override
other headers must go in

``<module>/public_overrides/...```

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

Public override headers - ``public_overrides/<module>``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
C++ implementation files go at the top level of the module. Implementation files
must always use "" style includes.

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

Command: ``pw module-check``
----------------------------

The ``pw module-check`` command exists to ensure that your module conforms to
the Pigweed module norms.

For example, at time of writing ``pw module-check pw_module`` is not passing
its own lint:

.. code-block:: none

  $ pw module-check pw_module

   ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
    ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
    ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
    ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
    ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀

  20191205 17:05:19 INF Checking module: pw_module
  20191205 17:05:19 ERR PWCK005: Missing ReST documentation; need at least e.g. "docs.rst"
  20191205 17:05:19 ERR FAIL: Found errors when checking module pw_module


