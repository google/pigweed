.. _module-pw_kernel-quickstart:

==========
Quickstart
==========
.. pigweed-module-subpage::
   :name: pw_kernel

.. note::

   This is an early draft. The content may change significantly over the
   next few months.

This quickstart shows you how to build, run, and demo ``pw_kernel`` within
:ref:`docs-glossary-upstream`.

We don't yet have documentation on how to pull ``pw_kernel`` into your project
as a dependency and build your system on top of it. Stay tuned!

-------------
Prerequisites
-------------
This quickstart assumes that you've got the :ref:`docs-glossary-upstream`
repository set up for development. If not, see :ref:`docs-contributing`.
Note that, since you aren't actually contributing code, you don't need to
do all the steps in the contributing guide. You only need to do these steps:

#. :ref:`docs-contributing-setup-devtools`
#. :ref:`docs-contributing-clone`

.. _module-pw_kernel-quickstart-config:

--------------------
Bazel configurations
--------------------
``pw_kernel`` provides several Bazel configurations for different targets and
build types:

- ``k_host``: For building and running on your host machine (Linux, macOS).
- ``k_qemu_mps2_an505``: For QEMU emulating an Arm Cortex-M33 based system (MPS2-AN505).
- ``k_qemu_virt_riscv32``: For QEMU emulating a RISC-V 32-bit based system.
- ``k_rp2350``: For the Raspberry Pi RP2350 microcontroller.

.. _module-pw_kernel-quickstart-build:

---------------
Build pw_kernel
---------------
To build ``pw_kernel`` for a given
:ref:`configuration <module-pw_kernel-quickstart-config>`:

.. code-block:: console

   bazelisk build --config <config> //pw_kernel/...

For example, to build for the RP2350:

.. code-block:: console

   bazelisk build --config k_rp2350 //pw_kernel/...

.. _module-pw_kernel-quickstart-test:

---------
Run tests
---------
To run all ``pw_kernel`` tests for a given
:ref:`configuration <module-pw_kernel-quickstart-config>`:

.. code-block:: console

   bazelisk test --config <config> //pw_kernel/...

For example, to run tests on the host:

.. code-block:: console

   bazelisk test --config k_host //pw_kernel/...

To run tests for the RISC-V QEMU target and see all test output:

.. code-block:: console

   bazelisk test --test_output=all --cache_test_results=no --config k_qemu_virt_riscv32 //pw_kernel/target/qemu_virt_riscv32:unittest_runner

---------------------
Run demo applications
---------------------
You can run pre-built demo applications in QEMU.

.. tip::

   To exit QEMU, press :kbd:`Ctrl+A` and then press :kbd:`X`.

QEMU for MPS2-AN505 (Cortex-M33)
================================
.. tab-set::

   .. tab-item:: Kernel-only demo

      .. code-block:: console

         bazelisk run --config k_qemu_mps2_an505 //pw_kernel/target/mps2_an505:kernel_only_demo

   .. tab-item:: Userspace demo

      .. code-block:: console

         bazelisk run --config k_qemu_mps2_an505 //pw_kernel/target/mps2_an505:userspace_demo

QEMU for virt (RISC-V)
======================
.. tab-set::

   .. tab-item:: Kernel-only demo

      .. code-block:: console

         bazelisk run --config k_qemu_virt_riscv32 //pw_kernel/target/qemu_virt_riscv32:kernel_only_demo

   .. tab-item:: Userspace demo

      .. code-block:: console

         bazelisk run --config k_qemu_virt_riscv32 //pw_kernel/target/qemu_virt_riscv32:userspace_demo

Raspberry Pi RP2350
===================
You can run the demos on a physical RP2350-based board (such as the Pico 2).

Build one of the demos:

.. tab-set::

   .. tab-item:: Kernel-only demo

      .. code-block:: console

         bazelisk build --config k_rp2350 //pw_kernel/target/pw_rp2350:kernel_only_demo

   .. tab-item:: Userspace demo

      .. code-block:: console

         bazelisk build --config k_rp2350 //pw_kernel/target/pw_rp2350:userspace_demo

The output ELF files will be located in the ``bazel-bin/pw_kernel/target/pw_rp2350/`` directory.

To view console output from the RP2350, connect to its serial port. The
following commands will build the firmware (if necessary) and then connect to
the device using
:ref:`pw_tokenizer.serial_detokenizer <module-pw_tokenizer-cli-detokenizing>`
to display human-readable logs. Replace ``<SERIAL_DEVICE>`` with your RP2350's
serial port, e.g. ``/dev/ttyACM0`` on Linux or ``/dev/cu.usbmodemXXXXXX`` on
macOS.

.. _probe-rs: https://probe.rs
.. _Installation: https://probe.rs/docs/getting-started/installation/

You can flash the compiled ``.elf`` files to the RP2350 using `probe-rs`_.
See `Installation`_.

Flash one of the demos:

.. tab-set::

   .. tab-item:: Kernel-only demo

      .. code-block:: console

         probe-rs download --chip rp2350 bazel-bin/pw_kernel/target/pw_rp2350/kernel_only_demo.elf && probe-rs reset --chip rp2350

   .. tab-item:: Userspace demo

      .. code-block:: console

         probe-rs download --chip rp2350 bazel-bin/pw_kernel/target/pw_rp2350/userspace_demo.elf && probe-rs reset --chip rp2350

Run one of the demos:

.. tab-set::

   .. tab-item:: Kernel-only demo

      .. code-block:: console

         bazelisk run --config k_rp2350 //pw_kernel/target/pw_rp2350:kernel_only_demo -- -d <SERIAL_DEVICE>

   .. tab-item:: Userspace demo

      .. code-block:: console

         bazelisk run --config k_rp2350 //pw_kernel/target/pw_rp2350:userspace_demo -- -d <SERIAL_DEVICE>

.. tip::

   For the best experience, start the console in one terminal window *before*
   flashing the device in another. This ensures you capture all log messages
   from boot and that the detokenizer uses the correct ELF database matching
   the flashed firmware.

-------------
VS Code setup
-------------
.. _rust-analyzer: https://rust-analyzer.github.io/

For the best Rust development experience, especially with VS Code, we recommend
using `rust-analyzer`_.

``rust-analyzer`` needs a ``rust-project.json`` file at the root of your workspace
to understand the project structure, dependencies, and build configurations.
You can generate this file using Bazel.

For a given :ref:`configuration <module-pw_kernel-quickstart-config>`:

.. code-block:: console

   bazelisk run @rules_rust//tools/rust_analyzer:gen_rust_project -- --config <config> //pw_kernel/...

Replace ``<config>`` with your chosen configuration, e.g. ``k_host``.

This command creates or updates the ``rust-project.json`` file in your Pigweed
project root.

To enable ``rust-analyzer`` to provide real-time feedback (errors and warnings)
in VS Code based on your Bazel build configuration, add the following to your
Pigweed project's ``.vscode/settings.json`` file.

.. code-block:: json

   "rust-analyzer.check.overrideCommand": [
     "bazelisk",
     "build",
     "--config=$CONFIG",
     "--@rules_rust//:error_format=json",
     "--experimental_ui_max_stdouterr_bytes=10485760",
     "//pw_kernel/..."
   ],
