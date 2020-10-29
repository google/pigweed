.. _docs-module-structure:

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

    # Exposed private headers go under internal/
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

C++ module structure
--------------------

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

C++ public override headers
~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

Compile-time configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~
Pigweed modules are intended to be used in a wide variety of environments.
In support of this, some modules expose compile-time configuration options.
Pigweed has an established pattern for declaring and overriding module
configuration.

.. tip::

  Compile-time configuration provides flexibility, but also imposes
  restrictions. A module can only have one configuration in a given build.
  This makes testing modules with compile-time configuration more difficult.
  Where appropriate, consider alternatives such as C++ templates or runtime
  configuration.

Declaring configuration
^^^^^^^^^^^^^^^^^^^^^^^
Configuration values are declared in a header file with macros. If the macro
value is not already defined, a default definition is provided. Otherwise,
nothing is done. Configuration headers may include ``static_assert`` statements
to validate configuration values.

.. code-block:: c++

  // Example configuration header

  #ifndef PW_FOO_INPUT_BUFFER_SIZE_BYTES
  #define PW_FOO_INPUT_BUFFER_SIZE_BYTES 128
  #endif  // PW_FOO_INPUT_BUFFER_SIZE_BYTES

  static_assert(PW_FOO_INPUT_BUFFER_SIZE_BYTES >= 64);

The configuration header may go in one of three places in the module, depending
on whether the header should be exposed by the module or not.

.. code-block::

  pw_foo/...

    # Publicly accessible configuration header
    public/pw_foo/config.h

    # Internal configuration header that is included by other module headers
    public/pw_foo/internal/config.h

    # Internal configuration header
    pw_foo_private/config.h

The configuration header is provided by a build system library. This library
acts as a :ref:`facade<docs-module-structure-facades>`. The facade uses a
variable such as ``pw_foo_CONFIG``. In upstream Pigweed, all config facades
default to the ``pw_build_DEFAULT_MODULE_CONFIG`` backend. In the GN build
system, the config facade is declared as follows:

.. code-block::

  declare_args() {
    # The build target that overrides the default configuration options for this
    # module. This should point to a source set that provides defines through a
    # public config (which may -include a file or add defines directly).
    pw_foo_CONFIG = pw_build_DEFAULT_MODULE_CONFIG
  }

  # An example source set for each potential config header location follows.

  # Publicly accessible configuration header (most common)
  pw_source_set("config") {
    public = [ "public/pw_foo/config.h" ]
    public_configs = [ ":public_include_path" ]
    public_deps = [ pw_foo_CONFIG ]
  }

  # Internal configuration header that is included by other module headers
  pw_source_set("config") {
    sources = [ "public/pw_foo/internal/config.h" ]
    public_configs = [ ":public_include_path" ]
    public_deps = [ pw_foo_CONFIG ]
    visibility = [":*"]  # Only allow this module to depend on ":config"
    friend = [":*"]  # Allow this module to access the config.h header.
  }

  # Internal configuration header
  pw_source_set("config") {
    public = [ "pw_foo_private/config.h" ]
    public_deps = [ pw_foo_CONFIG ]
    visibility = [":*"]  # Only allow this module to depend on ":config"
  }

Overriding configuration
^^^^^^^^^^^^^^^^^^^^^^^^
As noted above, all module configuration facades default to the same backend
(``pw_build_DEFAULT_MODULE_CONFIG``). This allows projects to override
configuration values for multiple modules from a single configuration backend,
if desired. The configuration values may also be overridden individually by
setting backends for the individual module configurations (e.g. in GN,
``pw_foo_CONFIG = "//configuration:my_foo_config"``).

Configurations are overridden by setting compilation options in the config
backend. These options could be set through macro definitions, such as
``-DPW_FOO_INPUT_BUFFER_SIZE_BYTES=256``, or in a header file included with the
``-include`` option.

This example shows two ways to configure a module in the GN build system.

.. code-block::

  # In the toolchain, set either pw_build_DEFAULT_MODULE_CONFIG or pw_foo_CONFIG
  pw_build_DEFAULT_MODULE_CONFIG = get_path_info(":define_overrides", "abspath")

  # This configuration sets PW_FOO_INPUT_BUFFER_SIZE_BYTES using the -D macro.
  pw_source_set("define_overrides") {
    public_configs = [ ":define_options" ]
  }

  config("define_options") {
    defines = [ "-DPW_FOO_INPUT_BUFFER_SIZE_BYTES=256" ]
  }

  # This configuration sets PW_FOO_INPUT_BUFFER_SIZE_BYTES with a header file.
  pw_source_set("include_overrides") {
    public_configs = [ ":header_options" ]

    # Header file with #define PW_FOO_INPUT_BUFFER_SIZE_BYTES 256
    sources = [ "my_config_overrides.h" ]
  }

  config("header_options") {
    cflags = [
      "-include",
      "my_config_overrides.h",
    ]
  }

.. _docs-module-structure-facades:

Facades
-------
In Pigweed, facades represent a dependency that can be swapped at compile time.
Facades are similar in concept to a virtual interface, but the implementation is
set by the build system. Runtime polymorphism with facades is not
possible, and each facade may only have one implementation (backend) per
toolchain compilation.

In the simplest sense, a facade is just a dependency represented by a variable.
For example, the ``pw_log`` facade is represented by the ``pw_log_BACKEND``
build variable. Facades typically are bundled with a build system library that
depends on the backend.

Facades are essential in some circumstances:

* Low-level, platform-specific features (:ref:`module-pw_cpu_exception`).
* Features that require a macro or non-virtual function interface
  (:ref:`module-pw_log`, :ref:`module-pw_assert`).
* Highly leveraged code where a virtual interface or callback is too costly or
  cumbersome (:ref:`module-pw_tokenizer`).

.. caution::

  Modules should only use facades when necessary. Facades are permanently locked
  to a particular implementation at compile time. Multpile backends cannot be
  used in one build, and runtime dependency injection is not possible, which
  makes testing difficult. Where appropriate, modules should use other
  mechanisms, such as virtual interfaces, callbacks, or templates, in place of
  facades.

The GN build system provides the
:ref:`pw_facade template<module-pw_build-facade>` as a convenient way to declare
facades.

Documentation
-------------
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

Creating a new Pigweed module
-----------------------------
To create a new Pigweed module, follow the below steps.

.. tip::

  Connect with the Pigweed community (by `mailing the Pigweed list
  <https://groups.google.com/forum/#!forum/pigweed>`_ or `raising your idea
  in the Pigweed chat <https://discord.gg/M9NSeTA>`_) to discuss your module
  idea before getting too far into the implementation. This can prevent
  accidentally duplicating work, or avoiding writing code that won't get
  accepted.

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

    - ``dir_pw_new = get_path_info("pw_new", "abspath")``

7. Add new module to main GN build

    - in ``/BUILD.gn`` to ``group("pw_modules")`` using folder alias variable

8. Add test target for new module in ``/BUILD.gn`` to
   ``pw_test_group("pw_module_tests")``
9. Add new module to CMake build

    - In ``/CMakeLists.txt`` add ``add_subdirectory(pw_new)``

10. Add the new module to docs module

    - Add in ``docs/BUILD.gn`` to ``pw_doc_gen("docs")``

11. Run :ref:`module-pw_module-module-check`

    - ``$ pw module-check {pw_module_dir}``

12. Contribute your module to upstream Pigweed (optional but encouraged!)
