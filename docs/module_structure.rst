.. _docs-module-structure:

----------------
Module Structure
----------------
The Pigweed :ref:`module <docs-glossary-module>` structure is designed to keep
as much code as possible for a particular slice of functionality in one place.
That means including the code from multiple languages, as well as all the
related documentation and tests.

Additionally, the structure is designed to limit the number of places a file
could go, so that when reading callsites it is obvious where a header is from.
That is where the duplicated ``<module>`` occurrences in file paths comes from.

Example module structure
------------------------
.. code-block:: text

   pw_foo/...

     docs.rst         # Docs landing page (required)
     concepts.rst     # Conceptual docs (optional)
     design.rst       # Design docs (optional)
     guides.rst       # How-to guides (optional)
     api.rst          # API reference (optional)
     cli.rst          # CLI reference (optional)
     gui.rst          # GUI reference (optional)
     tutorials/*.rst  # Tutorials (optional)

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
     py/BUILD.gn     # Python packages are declared in GN using pw_python_package
     py/setup.py     # Python files are structured as standard Python packages
     py/foo_test.py  # Tests go in py/ but outside of the Python package
     py/bar_test.py
     py/pw_foo/__init__.py
     py/pw_foo/__main__.py
     py/pw_foo/bar.py
     py/pw_foo/py.typed  # Indicates that this package has type annotations

     # Rust crates go into 'rust/...'
     rust/BUILD.bazel
     rust/crate_one.rs          # Single file crates are in rust/<crate_name>.rs
     rust/crate_two/lib.rs      # Multi-file crate's top level source in:
                                #   rust/<crate>/lib.rs
     rust/crate_two/mod_one.rs  # Multi-file crate's modules in:
     rust/crate_two/mod_two.rs  #   rust/<crate>/<module_name>.rs
                                # Prefer not using mod.rs files.

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
Located ``{pw_module_dir}/public/<module>``. These headers are the public
interface for the module.

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

     light_public_overrides/pw_unit_test/framework_backend.h
     googletest_public_overrides/pw_unit_test/framework_backend.h

     public_overrides/gtest
     public_overrides/gtest/gtest.h

     public/pw_unit_test
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

.. _module-structure-compile-time-configuration:

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
Configuration options are declared in a header file as macros. If the macro is
not already defined, a default definition is provided. Otherwise, nothing is
done. Configuration headers may include ``static_assert`` statements to validate
configuration values.

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
acts as a :ref:`facade<docs-module-structure-facades>`. The details depend on
the build system.

GN compile-time configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The facade uses a variable such as ``pw_foo_CONFIG``. In upstream Pigweed, all
config facades default to the ``pw_build_DEFAULT_MODULE_CONFIG`` backend. The
config facade is declared as follows:

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

Bazel compile-time configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The module that uses configuration depends on a ``label_flag``, conventionally
named ``config``, that by default points to the
``//pw_build:default_module_config``. For example,

.. code-block:: python

   # A module with a public config. That config doesn't need to be broken out
   # into a separate cc_library.
   cc_library(
     name = "pw_foo",
     hdrs = ["config.h"],
     deps = [":config"],
   )

   label_flag(
     name = "config",
     build_setting_default = "//pw_build:default_module_config",
   )

   # A module with an internal config that's included by other module headers.
   cc_library(
     name = "pw_bar",
     deps = [":internal_config"],
   )

   cc_library(
     name = "internal_config",
     hdrs = ["config.h"],
     deps = [":config"],
     visibility = ["//visibility:private"],
   )

   label_flag(
     name = "config",
     build_setting_default = "//pw_build:default_module_config",
   )

   # A module with a private config.
   cc_library(
     name = "pw_bar",
     implementation_deps = [":private_config"],
   )

   cc_library(
     name = "private_config",
     hdrs = ["config.h"],
     deps = [":config"],
     visibility = ["//visibility:private"],
   )

   label_flag(
     name = "config",
     build_setting_default = "//pw_build:default_module_config",
   )



Overriding configuration
^^^^^^^^^^^^^^^^^^^^^^^^
As noted above, all module configuration facades default to the same backend
(``pw_build_DEFAULT_MODULE_CONFIG`` in GN, ``//pw_build:default_module_config``
in Bazel). This allows projects to override configuration values for multiple
modules from a single configuration backend, if desired. The configuration
values may also be overridden individually by setting backends for the
individual module configurations (e.g. in GN, ``pw_foo_CONFIG =
"//configuration:my_foo_config"``, in Bazel
``--//pw_foo:config=//configuration:my_foo_config``).

Configurations options are overridden by setting macros in the config backend.
In Bazel, the only supported override mechanism are compilation options, such
as ``-DPW_FOO_INPUT_BUFFER_SIZE_BYTES=256``. In GN and CMake, configuration
macro definitions may also be set in a header file. The header file is included
using the ``-include`` compilation option.

GN config override example
..........................
This example shows two ways to configure a module in the GN build system.

.. code-block::

   # In the toolchain, set either pw_build_DEFAULT_MODULE_CONFIG or pw_foo_CONFIG
   pw_build_DEFAULT_MODULE_CONFIG = get_path_info(":define_overrides", "abspath")

   # This configuration sets PW_FOO_INPUT_BUFFER_SIZE_BYTES using the -D flag.
   pw_source_set("define_overrides") {
     public_configs = [ ":define_options" ]
   }

   config("define_options") {
     defines = [ "PW_FOO_INPUT_BUFFER_SIZE_BYTES=256" ]
   }

   # This configuration sets PW_FOO_INPUT_BUFFER_SIZE_BYTES in a header file.
   pw_source_set("include_overrides") {
     public_configs = [ ":set_options_in_header_file" ]

     # Header file with #define PW_FOO_INPUT_BUFFER_SIZE_BYTES 256
     sources = [ "my_config_overrides.h" ]
   }

   config("set_options_in_header_file") {
     cflags = [
       "-include",
       rebase_path("my_config_overrides.h", root_build_dir),
     ]
   }

Bazel config override example
.............................
This shows the only supported way to configure a module in Bazel.

.. code-block:: python

   # To use these overrides for all modules, set the global module config label
   # flag,
   #
   # --@pigweed//pw_build:default_module_config=//path_to:config_overrides
   #
   # To use them just for one module, set the module-specific config label
   # flag,
   #
   # --@pigweed//pw_foo:config_override=//path_to:config_overrides
   cc_library(
     name = "config_overrides",
     defines = [
        "PW_FOO_INPUT_BUFFER_SIZE_BYTES=256",
     ],
   )

To conditionally enable targets based on whether a particular config override is
enabled, a ``config_setting`` can be defined that looks at the config_override
label flag value. This allows use of ``target_compatible_with`` to enable
targets.

.. code-block:: python

   # Setup config_setting that is enabled when a particular config override is
   # set.
   config_setting(
     name = "config_override_setting",
     flag_values = {
       "--@pigweed//pw_foo:config_override": ":config_overrides",
     },
   )

   # For targets that need some specific config setting to build, conditionally
   # enable them.
   pw_cc_test(
     name = "test",
     target_compatible_with = select({
        ":config_override_setting": [],
        "//conditions:default": ["@platforms//:incompatible"],
     }),
     ...
   )


Why this config pattern is preferred
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Alternate patterns for configuring a module include overriding the module's
config header or having that header optionally include a header at a known
path (e.g. ``pw_foo/config_overrides.h``). There are a few downsides to these
approaches:

*  The module needs its own config header that defines, provides defaults for,
   and validates the configuration options. Replacing this header with a
   user-defined header would require defining all options in the user's header,
   which is cumbersome and brittle, and would bypass validation in the module's
   header.
*  Including a config override header at a particular path would prevent
   multiple modules from sharing the same configuration file. Multiple headers
   could redirect to the same configuration file, but this would still require
   creating a separate header and setting the config backend variable for each
   module.
*  Optionally including a config override header requires boilerplate code that
   would have to be duplicated in every configurable module.
*  An optional config override header file would silently be excluded if the
   file path were accidentally misspelled.

Python module structure
-----------------------
Python code is structured as described in the :ref:`docs-python-build-structure`
section of :ref:`docs-python-build`.

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
  to a particular implementation at compile time. Multiple backends cannot be
  used in one build, and runtime dependency injection is not possible, which
  makes testing difficult. Where appropriate, modules should use other
  mechanisms, such as virtual interfaces, callbacks, or templates, in place of
  facades.

The GN build system provides the
:ref:`pw_facade template<module-pw_build-facade>` as a convenient way to declare
facades.

Multiple Facades
~~~~~~~~~~~~~~~~
A module may contain multiple facades. Each facade's public override headers
must be contained in separate folders in the backend implementation, so that
it's possible to use multiple backends for a module.

.. code-block::

   # pw_foo contains 2 facades, foo and bar
   pw_foo/...
     # Public headers
     # public/pw_foo/foo.h #includes pw_foo_backend/foo.h
     # public/pw_foo/bar.h #includes pw_foo_backend/bar.h
     public/pw_foo/foo.h
     public/pw_foo/bar.h

   pw_foo_backend/...

     # Public override headers for facade1 and facade2 go in separate folders
     foo_public_overrides/pw_foo_backend/foo.h
     bar_public_overrides/pw_foo_backend/bar.h

Documentation
-------------
See :ref:`seed-0102`.

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

1. Create module folder following `Module name`_ guidelines.
2. Add `C++ public headers`_ files in
   ``{pw_module_dir}/public/{pw_module_name}/``
3. Add `C++ implementation files`_ files in ``{pw_module_dir}/``
4. Add module documentation

   - Add ``{pw_module_dir}/README.md`` that has a module summary
   - Add ``{pw_module_dir}/docs.rst`` that contains the main module
     documentation
   - Add optional documentation as described in :ref:`seed-0102`

5. Add GN build support in ``{pw_module_dir}/BUILD.gn``

   - Declare tests in ``pw_test_group("tests")``
   - Declare docs in ``pw_docs_group("docs")``

   Both ``tests`` and ``docs`` are required, even if the module is empty!

6. Add Bazel build support in ``{pw_module_dir}/BUILD.bazel``

7. Add CMake build support in ``{pw_module_dir}/CMakeLists.txt``

8. Add the new module to the ``/PIGWEED_MODULES`` list

   Modules must be listed one per line with no extra spaces or comments. This
   automatically adds the new module, its tests, and its docs, to the GN build.

9. Update the generated Pigweed modules lists file

   .. code-block:: bash

      ninja -C out update_modules

10. Add the new module to CMake build

    - In ``/CMakeLists.txt`` add ``add_subdirectory(pw_new)``

11. Run :ref:`module-pw_module-module-check`

    - ``$ pw module check {pw_module_dir}``

12. Contribute your module to upstream Pigweed (optional but encouraged!)
