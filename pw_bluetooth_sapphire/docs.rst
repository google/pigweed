.. _module-pw_bluetooth_sapphire:

=====================
pw_bluetooth_sapphire
=====================
.. pigweed-module::
   :name: pw_bluetooth_sapphire
   :tagline: Battle-tested Bluetooth with rock-solid reliability
   :status: unstable
   :languages: C++17
   :code-size-impact: 1.5 to 2 megabytes

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

-------
Roadmap
-------
* Support Bazel
* Support CMake
* Implement :ref:`module-pw_bluetooth` APIs
* Optimize memory footprint
* Add snoop log capture support
* Add metrics
* Add configuration options (LE only, Classic only, etc.)
* Add CLI for controlling stack over RPC
