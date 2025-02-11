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
* k_qemu_lm3s6965evb
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

Test
----

.. code-block:: shell

   bazelisk test --config k_qemu_lm3s6965evb //pw_kernel/...

Run
---

^A-x to exit qemu

.. code-block:: shell

   bazelisk run --config k_qemu_lm3s6965evb //pw_kernel/kernel/entry:kernel

RP2350 Target Board
===================

Build
-----

.. code-block:: shell

   bazelisk build --config k_rp2350 //pw_kernel/kernel/entry:kernel

Flash
-----

.. code-block:: shell

   probe-rs download --chip rp2350 bazel-bin/pw_kernel/kernel/entry/kernel && probe-rs reset
