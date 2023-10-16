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

.. _module-pw_emu-get-started:

-----------
Get started
-----------
Include the desired emulator target files in the ``pigweed.json`` configuration
file, e.g.:

.. code-block::

   ...
   "pw_emu": {
      "target_files": [

        "pw_emu/qemu-lm3s6965evb.json",
        "pw_emu/qemu-stm32vldiscovery.json",
        "pw_emu/qemu-netduinoplus2.json",
        "renode-stm32f4_discovery.json"
      ]
    }
    ...


Build the ``qemu_gcc`` target and use ``pw emu run`` command to run the target
binaries on the host using the ``qemu-lm3s6965evb`` emulation target:

.. code:: bash

   ninja -C out qemu_gcc
   pw emu run --args=-no-reboot qemu-lm3s6965evb out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_snapshot/test/cpp_compile_test

See :ref:`module-pw_emu-guide` for more examples,
:ref:`module-pw_emu-config` for detailed configuration information,
:ref:`module-pw_emu-cli` for detailed CLI usage information and
:ref:`module-pw_emu-api` for managing emulators with Python APIs.

.. toctree::
   :hidden:
   :maxdepth: 1

   guide
   config
   cli
   api
   design
