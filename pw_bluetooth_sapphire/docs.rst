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

---------------
Fuchsia support
---------------
``//pw_bluetooth_sapphire/fuchsia`` currently contains a stub bt-host to
demonstrate building, running, and testing Fuchsia components and packages with
the Fuchsia SDK.

It will eventually be filled with the real `bt-host component`_ once that's
migrated. See https://fxbug.dev/321267390.

Build the package
=================
To build the bt-host package, use one of the following commands:

.. code-block::

   bazel build //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.x64
   # OR
   bazel build //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.arm64

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

   bazel run //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.x64.component
   # OR
   bazel run //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.arm64.component

Run unit tests
==============
To run the bt-host unit tests, first start an emulator and then use the
following command:

.. code-block::

   bazel run //pw_bluetooth_sapphire/fuchsia/bt_host:test_pkg

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
Presubmits for bt-host are captured in a dedicated separate builder, rather than
existing ones such as ``pigweed-linux-bazel-noenv``.
These presubmits can be replicated with the following command:

.. code-block::

   bazel run //pw_bluetooth_sapphire/fuchsia:infra.test_all

.. note::
   You do not need to start an emulator beforehand to to run all tests this way.
   This test target will automatically provision one before running all tests.

Uploading to CIPD
=================
Pigweed infrastructure uploads bt-host's artifacts to
`fuchsia/prebuilt/bt-host`_ by building bt-host's top level infra target:

.. code-block::

   # Ensure all dependencies are built.
   bazel build //pw_bluetooth_sapphire/fuchsia:infra

   # Get builder manifest file.
   bazel build --output_groups=builder_manifest //pw_bluetooth_sapphire/fuchsia:infra

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
