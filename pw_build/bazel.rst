.. _module-pw_build-bazel:

Bazel
=====
Bazel is currently very experimental, and only builds for host and ARM Cortex-M
microcontrollers.

.. _module-pw_build-bazel-wrapper-rules:

Wrapper rules
-------------
The common configuration for Bazel for all modules is in the ``pigweed.bzl``
file. The built-in Bazel rules ``cc_binary``, ``cc_test`` are wrapped with
``pw_cc_binary`` and ``pw_cc_test``.

.. _module-pw_build-bazel-pw_linker_script:

pw_linker_script
----------------
In addition to wrapping the built-in rules, Pigweed also provides a custom
rule for handling linker scripts with Bazel. e.g.

.. code-block:: python

   pw_linker_script(
     name = "some_linker_script",
     linker_script = ":some_configurable_linker_script.ld",
     defines = [
         "PW_BOOT_FLASH_BEGIN=0x08000200",
         "PW_BOOT_FLASH_SIZE=1024K",
         "PW_BOOT_HEAP_SIZE=112K",
         "PW_BOOT_MIN_STACK_SIZE=1K",
         "PW_BOOT_RAM_BEGIN=0x20000000",
         "PW_BOOT_RAM_SIZE=192K",
         "PW_BOOT_VECTOR_TABLE_BEGIN=0x08000000",
         "PW_BOOT_VECTOR_TABLE_SIZE=512",
     ],
   )

   # You can include the linker script in the deps.
   cc_binary(
     name = "some_binary",
     srcs = ["some_source.cc"],
     deps = [":some_linker_script"],
   )

   # Alternatively, you can use additional_linker_inputs and linkopts. This
   # allows you to explicitly specify the command line order of linker scripts,
   # and may be useful if your project defines more than one.
   cc_binary(
     name = "some_binary",
     srcs = ["some_source.cc"],
     additional_linker_inputs = [":some_linker_script"],
     linkopts = ["-T $(location :some_linker_script)"],
   )

.. _module-pw_build-bazel-pw_facade:

pw_facade
---------
In Bazel, a :ref:`facade <docs-module-structure-facades>` module has a few
components:

#. The **facade target**, i.e. the interface to the module. This is what
   *backend implementations* depend on to know what interface they're supposed
   to implement.

#. The **library target**, i.e. both the facade (interface) and backend
   (implementation). This is what *users of the module* depend on. It's a
   regular ``cc_library`` that exposes the same headers as the facade, but
   has a dependency on the "backend label flag" (discussed next). It may also
   include some source files (if these are backend-independent).

   Both the facade and library targets are created using the
   ``pw_facade`` macro. For example, consider the following
   macro invocation:

   .. code-block:: python

      pw_facade(
          name = "binary_semaphore",
          # A backend-independent source file.
          srcs = [
              "binary_semaphore.cc",
          ],
          # The facade header.
          hdrs = [
              "public/pw_sync/binary_semaphore.h",
          ],
          # Dependencies of this header.
          deps = [
              "//pw_chrono:system_clock",
              "//pw_preprocessor",
          ],
          # The backend, hidden behind a label_flag; see below.
          backend = [
              ":binary_semaphore_backend",
          ],
      )

   This macro expands to both the library target, named ``binary_semaphore``,
   and the facade target, named ``binary_semaphore.facade``.

#. The **backend label flag**. This is a `label_flag
   <https://bazel.build/extending/config#label-typed-build-settings>`_: a
   dependency edge in the build graph that can be overridden by downstream projects.

#. The **backend target** implements a particular backend for a facade. It's
   just a plain ``cc_library``, with a dependency on the facade target. For example,

   .. code-block:: python

      cc_library(
          name = "binary_semaphore",
          srcs = [
              "binary_semaphore.cc",
          ],
          hdrs = [
              "public/pw_sync_stl/binary_semaphore_inline.h",
              "public/pw_sync_stl/binary_semaphore_native.h",
              "public_overrides/pw_sync_backend/binary_semaphore_inline.h",
              "public_overrides/pw_sync_backend/binary_semaphore_native.h",
          ],
          includes = [
              "public",
              "public_overrides",
          ],
          deps = [
              # Dependencies of the backend's headers and sources.
              "//pw_assert",
              "//pw_chrono:system_clock",
              # A dependency on the facade target, which defines the interface
              # this backend target implements.
              "//pw_sync:binary_semaphore.facade",
          ],
      )

   If a project uses only one backend for a given facade, the backend label
   flag should point at that backend target.

#. The **facade constraint setting** and **backend constraint values**. Every
   facade has an associated `constraint setting
   <https://bazel.build/concepts/platforms#api-review>`_ (enum used in platform
   definition), and each backend for this facade has an associated
   ``constraint_value`` (enum value). Example:

   .. code-block:: python

      # //pw_sync/BUILD.bazel
      constraint_setting(
        name = "binary_semaphore_backend_constraint_setting",
      )

      # //pw_sync_stl/BUILD.bazel
      constraint_value(
        name = "binary_semaphore_backend",
        constraint_setting = "//pw_sync:binary_semaphore_backend_constraint_setting",
      )

      # //pw_sync_freertos/BUILD.bazel
      constraint_value(
        name = "binary_semaphore_backend",
        constraint_setting = "//pw_sync:binary_semaphore_backend_constraint_setting",
      )

   `Target platforms <https://bazel.build/extending/platforms>`_ for Pigweed
   projects should indicate which backend they select for each facade by
   listing the corresponding ``constraint_value`` in their definition. This can
   be used in a couple of ways:

   #.  It allows projects to switch between multiple backends based only on the
       `target platform <https://bazel.build/extending/platforms>`_ using a
       *backend multiplexer* (see below) instead of setting label flags in
       their ``.bazelrc``.

   #.  It allows tests or libraries that only support a particular backend to
       express this through the `target_compatible_with
       <https://bazel.build/reference/be/common-definitions#common.target_compatible_with>`_
       attribute. Bazel will use this to `automatically skip incompatible
       targets in wildcard builds
       <https://bazel.build/extending/platforms#skipping-incompatible-targets>`_.

#. The **backend multiplexer**. If a project uses more than one backend for a
   given facade (e.g., it uses different backends for host and embedded target
   builds), the backend label flag will point to a target that resolves to the
   correct backend based on the target platform. This will typically be an
   `alias <https://bazel.build/reference/be/general#alias>`_ with a ``select``
   statement mapping constraint values to the appropriate backend targets. For
   example,

   .. code-block:: python

      alias(
          name = "pw_sync_binary_semaphore_backend_multiplexer",
          actual = select({
              "//pw_sync_stl:binary_semaphore_backend": "@pigweed//pw_sync_stl:binary_semaphore",
              "//pw_sync_freertos:binary_semaphore_backend": "@pigweed//pw_sync_freertos:binary_semaphore_backend",
              # If we're building for a host OS, use the STL backend.
              "@platforms//os:macos": "@pigweed//pw_sync_stl:binary_semaphore",
              "@platforms//os:linux": "@pigweed//pw_sync_stl:binary_semaphore",
              "@platforms//os:windows": "@pigweed//pw_sync_stl:binary_semaphore",
              # Unless the target platform is the host platform, it must
              # explicitly specify which backend to use. The unspecified_backend
              # is not compatible with any platform; taking this branch will produce
              # an informative error.
              "//conditions:default": "@pigweed//pw_build:unspecified_backend",
          }),
      )

pw_cc_blob_library
------------------
The ``pw_cc_blob_library`` rule is useful for embedding binary data into a
program. The rule takes in a mapping of symbol names to file paths, and
generates a set of C++ source and header files that embed the contents of the
passed-in files as arrays of ``std::byte``.

The blob byte arrays are constant initialized and are safe to access at any
time, including before ``main()``.

``pw_cc_blob_library`` is also available in the :ref:`GN <module-pw_build-cc_blob_library>`
and CMake builds.

Arguments
^^^^^^^^^
* ``blobs``: A list of ``pw_cc_blob_info`` targets, where each target
  corresponds to a binary blob to be transformed from file to byte array. This
  is a required field. ``pw_cc_blob_info`` attributes include:

  * ``symbol_name``: The C++ symbol for the byte array.
  * ``file_path``: The file path for the binary blob.
  * ``linker_section``: If present, places the byte array in the specified
    linker section.
  * ``alignas``: If present, uses the specified string verbatim in
    the ``alignas()`` specifier for the byte array.

* ``out_header``: The header file to generate. Users will include this file
  exactly as it is written here to reference the byte arrays.
* ``namespace``: C++ namespace to place the generated blobs within.
* ``alwayslink``: Whether this library should always be linked. Defaults to false.

Example
^^^^^^^
**BUILD.bazel**

.. code-block::

   pw_cc_blob_info(
     name = "foo_blob",
     file_path = "foo.bin",
     symbol_name = "kFooBlob",
   )

   pw_cc_blob_info(
     name = "bar_blob",
     file_path = "bar.bin",
     symbol_name = "kBarBlob",
     linker_section = ".bar_section",
   )

   pw_cc_blob_library(
     name = "foo_bar_blobs",
     blobs = [
       ":foo_blob",
       ":bar_blob",
     ],
     out_header = "my/stuff/foo_bar_blobs.h",
     namespace = "my::stuff",
   )

.. note:: If the binary blobs are generated as part of the build, be sure to
          list them as deps to the pw_cc_blob_library target.

**Generated Header**

.. code-block::

   #pragma once

   #include <array>
   #include <cstddef>

   namespace my::stuff {

   extern const std::array<std::byte, 100> kFooBlob;

   extern const std::array<std::byte, 50> kBarBlob;

   }  // namespace my::stuff

**Generated Source**

.. code-block::

   #include "my/stuff/foo_bar_blobs.h"

   #include <array>
   #include <cstddef>

   #include "pw_preprocessor/compiler.h"

   namespace my::stuff {

   const std::array<std::byte, 100> kFooBlob = { ... };

   PW_PLACE_IN_SECTION(".bar_section")
   const std::array<std::byte, 50> kBarBlob = { ... };

   }  // namespace my::stuff

.. _module-pw_build-bazel-pw_cc_binary_with_map:

pw_cc_binary_with_map
---------------------
The ``pw_cc_binary_with_map`` rule can be used to build a binary like
``cc_binary`` does but also generate a .map file from the linking step.

.. code-block::

   pw_cc_binary_with_map(
     name = "test",
     srcs = ["empty_main.cc"],
   )

This should result in a ``test.map`` file generated next to the ``test`` binary.

Note that it's only partially compatible with the ``cc_binary`` interface and
certain things are not implemented like make variable substitution.

Miscellaneous utilities
-----------------------

.. _module-pw_build-bazel-empty_cc_library:

empty_cc_library
^^^^^^^^^^^^^^^^
This empty library is used as a placeholder for label flags that need to point
to a library of some kind, but don't actually need the dependency to amount to
anything.

default_link_extra_lib
^^^^^^^^^^^^^^^^^^^^^^
This library groups together all libraries commonly required at link time by
Pigweed modules. See :ref:`docs-build_system-bazel_link-extra-lib` for more
details.

unspecified_backend
^^^^^^^^^^^^^^^^^^^
A special target used instead of a cc_library as the default condition in
backend multiplexer select statements to signal that a facade is in an
unconfigured state. This produces better error messages than e.g. using an
invalid label.

Toolchains and platforms
------------------------
Pigweed provides clang-based host toolchains for Linux and Mac Arm gcc
toolchain. The clang-based Linux and Arm gcc toolchains are entirely hermetic.
We don't currently provide a host toolchain for Windows.

