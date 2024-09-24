.. _module-pw_bluetooth_sapphire:

=====================
pw_bluetooth_sapphire
=====================
.. pigweed-module::
   :name: pw_bluetooth_sapphire

.. attention::
  This module is still under construction. There is no public API yet. Please
  contact the Pigweed team if you are interested in using this module.

The ``pw_bluetooth_sapphire`` module provides a dual-mode Bluetooth Host stack
that will implement the :ref:`module-pw_bluetooth` APIs.  Sapphire originated as
the Fuchsia operating system's Bluetooth stack and is used in production. The
Sapphire Host stack has been migrated into the Pigweed project for use in
embedded projects. However, as it was not written in an embedded context, there
is a lot of work to be done to optimize memory usage.

Why use Sapphire?

* **Open source**, unlike vendor Bluetooth stacks. This makes debugging and
  fixing issues much easier!
* **Integrated with Pigweed**. Logs, asserts, randomness, time, async, strings,
  and more are all using Pigweed modules.
* **Excellent test coverage**, unlike most Bluetooth stacks. This means fewer
  bugs and greater maintainability.
* **Certified**. Sapphire was certified by the Bluetooth SIG after passing
  all conformance tests.
* **A great API**. Coming 2024. See :ref:`module-pw_bluetooth`.

------------
Contributing
------------

Running tests
=============
.. tab-set::

   .. tab-item:: Bazel (host)

      Run all tests:

      .. code-block:: console

         bazel test //pw_bluetooth_sapphire/host/... \
           --platforms=//pw_unit_test:googletest_platform \
           --@pigweed//pw_unit_test:backend=@pigweed//pw_unit_test:googletest

      Run l2cap tests with a test filter, logs, and log level filter:

      .. code-block:: console

         bazel test //pw_bluetooth_sapphire/host/l2cap:l2cap_test \
           --platforms=//pw_unit_test:googletest_platform \
           --@pigweed//pw_unit_test:backend=@pigweed//pw_unit_test:googletest \
           --test_arg=--gtest_filter="*InboundChannelFailure" \
           --test_output=all \
           --copt=-DPW_LOG_LEVEL_DEFAULT=PW_LOG_LEVEL_ERROR

   .. tab-item:: Bazel (Fuchsia emulator)

      .. code-block:: console

         bazel run --config=fuchsia //pw_bluetooth_sapphire/fuchsia:infra.test_all

   .. tab-item:: GN (host)

      The easiest way to run GN tests is with ``pw presubmit``, which will install
      dependencies and set GN args correctly.

      .. code-block:: console

         $ pw presubmit --step gn_chre_googletest_nanopb_sapphire_build

      You can also use ``pw watch`` if you install all the packages and set
      your GN args to match the `GN presubmit step`_.

Clangd configuration
====================
Currently some manual steps are required to get clangd working with Fuchsia
code (for example, for FIDL server files).

1. Execute ``bazel run //:refresh_compile_commands_for_fuchsia_sdk`` to
   generate ``compile_commands.json``. This needs to be done whenever the build
   graph changes.
2. Add this flag to your clangd configuration, fixing the full path to your
   Pigweed checkout:
   ``--compile-commands-dir=/path/to/pigweed/.compile_commands/fuchsia``

---------------
Fuchsia support
---------------
``//pw_bluetooth_sapphire/fuchsia`` currently contains a stub bt-host to
demonstrate building, running, and testing Fuchsia components and packages with
the Fuchsia SDK.

.. note::
   Please do not add any fuchsia-specific dependencies (targets that load from
   ``@fuchsia_sdk``) outside of ``//pw_bluetooth_sapphire/fuchsia`` since that
   will break the global pigweed build (``//...``) for macos hosts.

.. note::
   Every ``bazel`` invocation needs ``--config=fuchsia`` whenever the target or
   dependency needs to specify ``@fuchsia_sdk`` backends for pigweed and the
   target platform is fuchsia.

It will eventually be filled with the real `bt-host component`_ once that's
migrated. See https://fxbug.dev/321267390.

Build the package
=================
To build the bt-host package, use one of the following commands:

.. code-block::

   bazel build --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.x64
   # OR
   bazel build --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.arm64

Start an emulator
=================
To run an emulator, use one of the following commands:

.. code-block::

   bazel run @fuchsia_products//:core.x64.emu -- --headless
   # OR
   bazel run @fuchsia_products//:minimal.arm64.emu -- --headless

Flash a device
==============
To flash a vim3, use the following command:

.. code-block::

   bazel run @fuchsia_products//:core.vim3.flash -- --target <device_serial_num>

Run the component
=================
To run the bt-host component, first provision a Fuchsia target and then use one
of the following command:

.. code-block::

   bazel run --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.x64.component
   # OR
   bazel run --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.arm64.component

Run unit tests
==============
To run the bt-host unit tests, first start an emulator and then use the
following command:

.. code-block::

   bazel run --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:test_pkg

Stop the emulator
=================
To stop the running emulator, use the following command:

.. code-block::

   pw ffx emu stop --all

Add a target test to presubmits
===============================
To add new test packages to presubmits, add the test package targets to
``//pw_bluetooth_sapphire/fuchsia/BUILD.bazel``.

Example:

.. code-block::

   # pw_bluetooth_sapphire/fuchsia/BUILD.bazel

   qemu_tests = [
       "//pw_bluetooth_sapphire/fuchsia/bt_host:integration_test_pkg",
       ...
   ]

Run Fuchsia presubmit tests
===========================
Presubmits for bt-host are captured in a dedicated separate builder,
``pigweed-linux-bazel-bthost``, rather than existing ones such as
``pigweed-linux-bazel-noenv``.

On the builder invocation console, there are a number of useful artifacts for
examining the environment during test failures. Here are some notable examples:

* ``bt_host_package`` stdout: Combined stdout/stderr of the entire test orchestration and execution.
* ``subrunner.log``: Combined test stdout/stderr of test execution only.
* ``target.log``: The ffx target device's logs.
* ``ffx_config.txt``: The ffx configuration used for provisioning and testing.
* ``ffx.log``: The ffx host logs.
* ``ffx_daemon.log``: The ffx daemon's logs.
* ``env.dump.txt``: The environment variables when test execution started.
* ``ssh.log``: The ssh logs when communicating with the target device.

These presubmits can be also be replicated locally with the following command:

.. code-block::

   bazel run --config=fuchsia //pw_bluetooth_sapphire/fuchsia:infra.test_all

.. note::
   Existing package servers must be stopped before running this command. To
   check for any existing package servers run ``lsof -i :8083`` and make sure
   each of those processes are killed.

.. note::
   You do not need to start an emulator beforehand to to run all tests this way.
   This test target will automatically provision one before running all tests.

Uploading to CIPD
=================
Pigweed infrastructure uploads bt-host's artifacts to
`fuchsia/prebuilt/bt-host`_ by building bt-host's top level infra target:

.. code-block::

   # Ensure all dependencies are built.
   bazel build --config=fuchsia //pw_bluetooth_sapphire/fuchsia:infra

   # Get builder manifest file.
   bazel build --config=fuchsia --output_groups=builder_manifest //pw_bluetooth_sapphire/fuchsia:infra

The resulting file contains a ``cipd_manifests`` json field which references a
sequence of json files specifying the CIPD package path and package file
contents.

-------
Roadmap
-------
* Support Bazel (In Progress)
* Support CMake
* Implement :ref:`module-pw_bluetooth` APIs
* Optimize memory footprint
* Add snoop log capture support
* Add metrics
* Add configuration options (LE only, Classic only, etc.)
* Add CLI for controlling stack over RPC

.. _bt-host component: https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/connectivity/bluetooth/core/bt-host/
.. _fuchsia/prebuilt/bt-host: https://chrome-infra-packages.appspot.com/p/fuchsia/prebuilt/bt-host
.. _GN presubmit step: https://cs.opensource.google/pigweed/pigweed/+/main:pw_presubmit/py/pw_presubmit/pigweed_presubmit.py?q=gn_chre_googletest_nanopb_sapphire_build
