.. _module-pw_bluetooth_sapphire-fuchsia:

===================
Fuchsia development
===================
.. pigweed-module-subpage::
   :name: pw_bluetooth_sapphire

``//pw_bluetooth_sapphire/fuchsia`` currently contains the Fuchsia build
targets for building, running, and testing the ``bt-host`` and
``bt-hci-virtual`` packages.

Fuchsia's Sapphire Bluetooth stack integration uses the ``bt-host`` core
package which is built from Pigweed CI, uploaded to CIPD, and rolled
automatically. The ``bt-host`` package is assembled into products in any
Fuchsia build which uses Bluetooth. The ``bt-hci-virtual`` package is included
in some builds for testing. See `bt-host README
<https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/connectivity/bluetooth/core/bt-host/README.md>`__
for more details on product assembly.

.. note::
   Every ``bazelisk`` invocation needs ``--config=fuchsia`` whenever the target
   or dependency needs to specify ``@fuchsia_sdk`` backends for Pigweed and the
   target platform is Fuchsia.

----------------------------------------
Accessing ffx from a Pigweed environment
----------------------------------------
The ``ffx`` command is available as a subcommand of ``pw``:

.. code-block:: console

   pw ffx help

-------
Testing
-------

Running tests
=============
First, ensure the emulator is running or hardware running Fuchsia is
connected. Then run a test package with:

.. code-block:: console

   bazelisk run --config=fuchsia //pw_bluetooth_sapphire/fuchsia/host/l2cap:test_pkg

Flags can also be passed to the test binary:

.. code-block:: console

   bazelisk run --config=fuchsia //pw_bluetooth_sapphire/fuchsia/host/l2cap:test_pkg \
     -- --gtest-filter="*Example" --severity=DEBUG

.. note::
   If the test is unable to connect to the emulator, run ``pw ffx target
   remove --all`` first to clean your machine's target list.

You can also run the presubmit step that will start an emulator and run
all tests, but this is slow:

.. code-block:: console

   pw presubmit --step bthost_package

Emulator
========
To start the emulator, use one of the following commands:

.. code-block::

   bazelisk run @fuchsia_products//:core.x64.emu -- --headless
   # OR
   bazelisk run @fuchsia_products//:minimal.arm64.emu -- --headless

To stop the running emulator, use the following command:

.. code-block::

   pw ffx emu stop --all

--------
Building
--------
To build the ``bt-host`` package, use one of the following commands:

.. tab-set::

   .. tab-item:: arm64

      .. code-block::

         bazelisk build --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.arm64

   .. tab-item:: x64

      .. code-block::

         bazelisk build --config=fuchsia //pw_bluetooth_sapphire/fuchsia/bt_host:pkg.x64

The ``bt-host.far`` package will end up in a Bazel build directory that will be
printed in the command output. For example:
``bazel-out/aarch64-fastbuild-e2b/bin/pw_bluetooth_sapphire/fuchsia/bt_host/bt-host.far``.
Note that ``bazel-out`` is symlinked from the root Pigweed directory.

Use the prebuilt in fuchsia.git
===============================
fuchsia.git developers can copy/link the ``bt-host.far`` file to
``//prebuilt/connectivity/bluetooth/bt-host/<arch>/`` and rename it to
``bt-host`` to replace the prebuilt that Fuchsia uses.

--------------------
Working with devices
--------------------

Inspect
=======
To query the current state of the ``bt-host`` component Inspect hierarchy, run:

#. ``pw ffx inspect list | grep bt-host`` to find the component's ``<moniker>``

#. ``pw ffx inspect show "<moniker>"``

   * Note that the full moniker from step 2 should be in quotations, e.g.
     ``pw ffx inspect show "core/bluetooth-core/bt-host-collection\:bt-host_000"``.

   * Wildcards can be passed into the selector as needed, e.g.
     ``pw ffx inspect show "core/bluetooth-core/bt-host-collection*"``.

--------------------
Editor configuration
--------------------

Clangd
======
Currently some manual steps are required to get clangd working with Fuchsia
code (for example, for FIDL server files).

#. Execute the following command to generate ``compile_commands.json``. This
   needs to be done whenever the build graph changes.

   .. code-block:: console

      bazelisk run //:refresh_compile_commands_for_fuchsia_sdk

#. Add this flag to your clangd configuration, fixing the full path to your
   Pigweed checkout:

   .. code-block:: console

      --compile-commands-dir=/path/to/pigweed/.compile_commands/fuchsia

--------------
Infrastructure
--------------

Run Fuchsia presubmit tests
===========================
Presubmits for ``bt-host`` are captured in a dedicated separate builder,
``pigweed-linux-bazel-bthost``, rather than existing ones such as
``pigweed-linux-bazel-noenv``.

On the builder invocation console, there are a number of useful artifacts for
examining the environment during test failures. Here are some notable examples:

* ``bt_host_package`` ``stdout``: Combined ``stdout``/``stderr`` of the entire test orchestration and execution.
* ``subrunner.log``: Combined test ``stdout``/``stderr`` of test execution only.
* ``target.log``: The ``ffx`` target device's logs.
* ``ffx_config.txt``: The ``ffx`` configuration used for provisioning and testing.
* ``ffx.log``: The ``ffx`` host logs.
* ``ffx_daemon.log``: The ``ffx`` daemon's logs.
* ``env.dump.txt``: The environment variables when test execution started.
* ``ssh.log``: The SSH logs when communicating with the target device.

These presubmits can be also be replicated locally with the following command:

.. code-block::

   bazelisk run --config=fuchsia //pw_bluetooth_sapphire/fuchsia:infra.test_all

.. note::
   Existing package servers must be stopped before running this command. To
   check for any existing package servers run ``lsof -i :8083`` and make sure
   each of those processes are killed.

.. note::
   You do not need to start an emulator beforehand to to run all tests this way.
   This test target will automatically provision one before running all tests.

Add a test to presubmit
=======================
Fuchsia test packages are those defined with a Fuchsia SDK rule like
``fuchsia_unittest_package``. All Fuchsia test packages need to be added to the
Fuchsia presubmit step or they will not be tested.

To add new Fuchsia test packages to presubmit, add the test package targets to
``//pw_bluetooth_sapphire/fuchsia/BUILD.bazel``.

Example:

.. code-block::

   # pw_bluetooth_sapphire/fuchsia/BUILD.bazel

   qemu_tests = [
       "//pw_bluetooth_sapphire/fuchsia/bt_host:integration_test_pkg",
       ...
   ]

Uploading to CIPD
=================
Pigweed infrastructure uploads ``bt-host`` artifacts to
`fuchsia/prebuilt/bt-host`_ and `fuchsia/prebuilt/bt-hci-virtual`_ via the
`pigweed-linux-bazel-bthost`_ builder by building the top level infra target:

.. code-block::

   # Ensure all dependencies are built.
   bazelisk build --config=fuchsia //pw_bluetooth_sapphire/fuchsia:infra

   # Get builder manifest file.
   bazelisk build --config=fuchsia --output_groups=builder_manifest //pw_bluetooth_sapphire/fuchsia:infra

The resulting file contains a ``cipd_manifests`` JSON field which references a
sequence of JSON files specifying the CIPD package path and package file
contents.

.. _fuchsia/prebuilt/bt-host: https://chrome-infra-packages.appspot.com/p/fuchsia/prebuilt/bt-host
.. _fuchsia/prebuilt/bt-hci-virtual: https://chrome-infra-packages.appspot.com/p/fuchsia/prebuilt/bt-hci-virtual
.. _pigweed-linux-bazel-bthost: https://ci.chromium.org/ui/p/pigweed/builders/pigweed.ci/pigweed-linux-bazel-bthost
