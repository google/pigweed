.. _module-pw_kernel-cheat-sheet:

===========
Cheat Sheet
===========
.. pigweed-module-subpage::
   :name: pw_kernel

.. _module-pw_kernel-build:

-------------
rust_analyzer
-------------

Generating rust-project.json
============================

For a given ``$CONFIG`` in:

* k_host
* k_qemu_mps2_an505
* k_qemu_virt_riscv32
* k_rp2350

.. code-block:: shell

   bazelisk run @rules_rust//tools/rust_analyzer:gen_rust_project -- --config $CONFIG //pw_kernel/...


Errors and warnings in VSCode
=============================

Add this to Pigweed's ``.vscode/settings.json``.  Note that it only builds the
kernel targets to limit the amount of time that it takes to run.  Substitute
``$CONFIG`` for the config chosen above.

.. code-block:: json

   "rust-analyzer.check.overrideCommand": [
     "bazelisk",
     "build",
     "--config=$CONFIG",
     "--@rules_rust//:error_format=json",
     "//pw_kernel/..."
   ],

--------------
Build and Test
--------------

Host
====

Test
----

.. code-block:: shell

   bazelisk test --config k_host //pw_kernel/...

QEMU
====

Supported QEMU platforms:
* ``k_qemu_mps2_an505`` - a cortex-m33 based system
* ``k_qemu_virt_riscv32`` - a riscv32 based system

Test
----

.. code-block:: shell

   bazelisk test --config k_qemu_mps2_an505 //pw_kernel/...

.. code-block:: shell

   bazelisk test --config k_qemu_virt_riscv32 //pw_kernel/...

Run
---

^A-x to exit qemu

.. code-block:: shell

   bazelisk run --config k_qemu_mps2_an505 //pw_kernel/target/mps2_an505:kernel_only_demo
   bazelisk run --config k_qemu_mps2_an505 //pw_kernel/target/mps2_an505:userspace_demo

.. code-block:: shell

   bazelisk run --config k_qemu_virt_riscv32 //pw_kernel/target/qemu_virt_riscv32:kernel_only_demo

RP2350 Target Board
===================

Build
-----

.. code-block:: shell

   bazelisk build --config k_rp2350 //pw_kernel/target/pw_rp2350:kernel_only_demo
   bazelisk build --config k_rp2350 //pw_kernel/target/pw_rp2350:userspace_demo

Console
---

.. code-block:: shell

   bazelisk run --config k_rp2350 //pw_kernel/target/pw_rp2350:kernel_only_demo -- -d <SERIAL_DEVICE>
   bazelisk run --config k_rp2350 //pw_kernel/target/pw_rp2350:userspace_demo -- -d <SERIAL_DEVICE>

Running the console will trigger a build of the kernel if required.

Flash
-----

.. code-block:: shell

   probe-rs download --chip rp2350 bazel-bin/pw_kernel/target/pw_rp2350/kernel_only_demo && probe-rs reset
   probe-rs download --chip rp2350 bazel-bin/pw_kernel/target/pw_rp2350/userspace_demo && probe-rs reset

Note that any logging messages between boot and connecting a console to the device will be missed,
so it's best to start the console in one terminal first, before flashing the device.  This will also
ensure that the image that's flashed to the device matches the image that's being used to detokenize
the logs.
