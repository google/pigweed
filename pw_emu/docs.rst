.. _module-pw_emu:

.. rst-class:: with-subtitle

======
pw_emu
======
.. pigweed-module::
   :name: pw_emu
   :tagline: Emulators frontend
   :status: experimental

--------
Overview
--------
``pw_emu`` is an emulators frontend with the following features:

* It allows users to define an emulation target that encapsulates the emulated
  machine configuration, the tools configuration and the host channels
  configuration.

* It provides a command line interface that manages multiple emulator instances
  and provides interactive access to the emulator's host channels.

* It provides a Python API to control emulator instances and access the
  emulator's host channels (e.g. serial ports).

* It supports multiple emulators, QEMU and renode.

* It expose channels for gdb, monitor and user selected devices through
  configurable host resources like sockets and ptys.

For background on why the module exists and some of the design
choices see :ref:`seed-0108`.


.. toctree::
   :hidden:
   :maxdepth: 1

   guide
   config
   cli
   api
   design
