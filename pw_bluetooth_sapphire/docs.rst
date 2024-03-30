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
``//pw_bluetooth_sapphire/fuchsia`` currently contains stub boilerplate to
demonstrate building, running, and testing Fuchsia components and packages with
the Fuchsia SDK.

It will eventually be replaced by the `bt-host component`_ once that's migrated.

Build the package
=================
To build the example stub package, use the following command:

.. code-block::

   bazel build //pw_bluetooth_sapphire/fuchsia/hello_world:pkg

Start an emulator
=================
To run an emulator, use the following command:

.. code-block::

   bazel run @fuchsia_products//:core.x64.emu -- --headless

Run the component
=================
To run the example stub component, first start an emulator and then use the
following command:

.. code-block::

   bazel run //pw_bluetooth_sapphire/fuchsia/hello_world:pkg.component

Run unit tests
==============
To run the example component's unit tests, first start an emulator and then use
the following command:

.. code-block::

   bazel run //pw_bluetooth_sapphire/fuchsia/hello_world:test_pkg

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
       "//pw_bluetooth_sapphire/fuchsia/src:test_pkg",
       ...
   ]

Run Fuchsia presubmit tests
===========================
To run the Fuchsia target tests that are typically run under presubmit, use the
following command.

.. code-block::

   bazel run //pw_bluetooth_sapphire/fuchsia:tests_qemu

.. note::
   You do not need to start an emulator beforehand to to run all tests this way.
   This test target will automatically provision one before running all tests.

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
