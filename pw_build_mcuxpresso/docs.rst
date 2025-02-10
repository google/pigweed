.. _module-pw_build_mcuxpresso:

===================
pw_build_mcuxpresso
===================
.. pigweed-module::
   :name: pw_build_mcuxpresso

The ``pw_build_mcuxpresso`` module provides helper utilities for building a
target based on an NXP MCUXpresso SDK.

The GN build files live in ``third_party/mcuxpresso`` but are documented here.
The rationale for keeping the build files in ``third_party`` is that code
depending on an MCUXpresso SDK can clearly see that their dependency is on
third party, not pigweed code.

-----------------------
Using an MCUXpresso SDK
-----------------------
An MCUXpresso SDK consists of a number of components, each of which has a set
of sources, headers, preprocessor defines, and dependencies on other
components. These are all described in an XML "manifest" file included in the
SDK package.

To use the SDK within a Pigweed project, the set of components you need must be
combined into a library that you can depend on. This library will include all of
the sources and headers, along with necessary preprocessor defines, for those
components and their dependencies.

Optional components
===================
Including components will include all of their required dependencies. Where the
components you include have optional dependencies, they must be satisfied by the
set of components you include otherwise the library generation will fail with an
error.

Excluding components
====================
Components can be excluded from the generated source set, for example to
suppress errors about optional dependencies your project does not need, or to
prevent an unwanted component dependency from being introduced into your
project.

mcuxpresso_builder
==================
``mcuxpresso_builder`` is a utility installed into the environment that is used
by the GN build scripts in ``third_party/mcuxpresso``, or directly by you to
generate rules for the Bazel build.

Usage is documented for each build system in the relevant section.

------------
The GN build
------------
Using an MCUxpresso SDK within a Pigweed project that uses the GN Build system
involves the creation of one or more ``pw_source_set`` targets you can depend on
in your executable targets.

These source sets sets are defined using the ``pw_mcuxpresso_sdk`` template.
Provide the path to the ``manifest`` XML, along with the names of the components
you wish to ``include``.

For boards with multiple cores, pass the specific core to filter components for
in ``device_core``.

.. code-block:: text

   import("$dir_pw_third_party/mcuxpresso/mcuxpresso.gni")

   pw_mcuxpresso_sdk("sample_project_sdk") {
     manifest = "$dir_pw_third_party/mcuxpresso/evkmimxrt595/EVK-MIMXRT595_manifest_v3_13.xml"
     include = [
       "component.serial_manager_uart.MIMXRT595S",
       "project_template.evkmimxrt595.MIMXRT595S",
       "utility.debug_console.MIMXRT595S",
     ]
     device_core = "cm33_MIMXRT595S"
   }

   pw_executable("hello_world") {
     sources = [ "hello_world.cc" ]
     deps = [ ":sample_project_sdk" ]
   }

To exclude components, provide the list to ``exclude`` as an argument to the
template. For example to replace the FreeRTOS kernel bundled with the MCUXpresso
SDK with the Pigweed third-party target:

.. code-block:: text

   pw_mcuxpresso_sdk("freertos_project_sdk") {
     // manifest and includes ommitted for clarity
     exclude = [ "middleware.freertos-kernel.MIMXRT595S" ]
     public_deps = [ "$dir_pw_third_party/freertos" ]
   }

Introducing dependencies
========================
As seen above, the generated source set can have dependencies added by passing
the ``public_deps`` (or ``deps``) arguments to the template.

You can also pass the ``allow_circular_includes_from``, ``configs``, and
``public_configs`` arguments to augment the generated source set.

For example it is very common to replace the ``project_template`` component with
a source set of your own that provides modified copies of the files from the
SDK.

To resolve circular dependencies, in addition to the generated source set, two
configs named with the ``__defines`` and ``__includes`` suffixes on the template
name are generated, to provide the preprocessor defines and include paths that
the source set uses.

.. code-block:: text

   pw_mcuxpresso_sdk("my_project_sdk") {
     manifest = "$dir_pw_third_party/mcuxpresso/evkmimxrt595/EVK-MIMXRT595_manifest_v3_13.xml"
     include = [
       "component.serial_manager_uart.MIMXRT595S",
       "utility.debug_console.MIMXRT595S",
     ]
     public_deps = [ ":my_project_config" ]
     allow_circular_includes_from = [ ":my_project_config" ]
   }

   pw_source_set("my_project_config") {
     sources = [ "board.c", "clock_config.c", "pin_mux.c" ]
     public = [ "board.h", "clock_config.h", "pin_mux.h "]
     public_configs = [
       ":my_project_sdk__defines",
       ":my_project_sdk__includes"
     ]
   }

mcuxpresso_builder
==================
For the GN build, this utility is invoked by the ``pw_mcuxpresso_sdk`` template.
You should only need to interact with ``mcuxpresso_builder`` directly if you are
doing something custom.

This command generates repository that contains BUILD rules for both GN and Bazel.
You can use `--skip-bazel` or `--skip-gn` to skip generating rules for respective
build system.

.. code-block:: bash

   mcuxpresso_builder EVK-MIMXRT595_manifest_v3_14.xml \
     --include project_template.evkmimxrt595.MIMXRT595S \
     utility.debug_console.MIMXRT595S \
   component.serial_manager_uart.MIMXRT595S \
   --exclude middleware.freertos-kernel.MIMXRT595S \
     --device-core cm33_MIMXRT595S \
     --output-path gn_out_sdk \
   --mcuxpresso-repo https://github.com/nxp-mcuxpresso/mcux-sdk \
   --mcuxpresso-rev MCUX_2.16.000

---------------
The Bazel build
---------------
Using an MCUxpresso SDK within a Pigweed project that uses the Bazel build
system involves the creation of one or more ``cc_library`` targets you can
depend on in your executable targets.

These targets should select required components from the SDK using the pre-generated
``BUILD.bazel`` file created from SDK manifest.

Out of the box, Pigweed provides rules for basic components from
the MCUXpresso SDK. You can list those components out by running

.. code-block:: sh

   bazelisk query @mcuxpresso//...


To use those components, simply specify them as deps in your code.

.. code-block:: python

   cc_library(
     name = "mcuxpresso_sdk",
     target_compatible_with = [
       "@platforms//cpu:armv8-m",
     ],
     deps = [
       "@mcuxpresso//:component.serial_manager_uart.MIMXRT595S",
       "@mcuxpresso//:utility.debug_console.MIMXRT595S",
     ],
   )

In addition, you might want to pass some additional configuration
to SDK rules. You can do that by overriding the ``@mcuxpresso//:user_config``
option to point to your custom rule

.. code-block:: python

   config_setting(
     name = "debug",
     flag_values = {"@mcuxpresso//:user_config": "//:my_sdk_config"}
   )

   cc_library(
     name = "my_sdk_config",
     defines = [
       "CPU_MIMXRT595SFFOC_cm33",
       "SDK_DEBUGCONSOLE=1",
     ],
   )


Generating the SDK
==================
If your use case requires you to use components that are not provided
by Pigweed, you will have to use the ``mcuxpresso_builder`` script
to generate additional targets for these components.

Provide the path to the manifest XML, url to MCUxpresso SDK repository
along with the names of the components you wish to
``--include`` or ``--exclude``.

This command generates repository that contains BUILD rules for both GN and Bazel.
You can use `--skip-bazel` or `--skip-gn` to skip generating rules for respective
build system.

.. code-block:: bash

   bazelisk run //pw_build_mcuxpresso/py:mcuxpresso_builder -- EVK-MIMXRT595_manifest_v3_14.xml \
     --mcuxpresso-repo=https://github.com/nxp-mcuxpresso/mcux-sdk \
     --mcuxpresso-rev=MCUX_2.16.000 \
     --device-core=cm33_MIMXRT595S \
     --output-path=bazel-out/k8-fastbuild/bin/mcuxpresso-sdk \
     --clean \
     --include \
     project_template.evkmimxrt595.MIMXRT595S \
     utility.debug_console.MIMXRT595S \
     component.serial_manager_uart.MIMXRT595S \
     --exclude \
     middleware.freertos-kernel.MIMXRT595S


This will generate a new SDK together with a Bazel build file containing rules
for each of the specified components (and their dependencies) and
a ``README.md`` file with additional information.

After that, update ``MODULE.bazel`` to point to your
generated SDK.

.. code-block:: python

   new_git_repository(
     name = "mcuxpresso",
     commit = "your_commit_sha",
     remote = "your_remote",
   )

Directly modifying the generated SDK is not recommended.
