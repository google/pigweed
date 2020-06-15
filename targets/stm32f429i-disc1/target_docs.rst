.. _chapter-stm32f429i-disc1:

.. default-domain:: cpp

.. highlight:: sh

----------------
stm32f429i-disc1
----------------
The STMicroelectronics STM32F429I-DISC1 development board is currently Pigweed's
primary target for on-device testing and development.

Building
========
To build for this Pigweed target, simply build the top-level "stm32f429i" Ninja
target.

.. code:: sh

  $ ninja -C out stm32f429i

Testing
=======
When working in upstream Pigweed, building this target will build all Pigweed modules' unit tests.
These tests can be run on-device in a few different ways.

Run a unit test
---------------
If using ``out/disco`` as a build directory, tests will be located in
``out/stm32f429i/obj/[module name]/[test_name].elf``. To run these on device,
the stm32f429i-disc1 target provides a helper script that flashes the test to a
device and then runs it.

.. code:: sh

  # Setup pigweed environment.
  $ . pw_env_setup/setup.sh
  # Run test.
  $ stm32f429i_disc1_unit_test_runner /path/to/binary

Run multiple tests
------------------
Running all tests one-by-one is rather tedious. To make running multiple
tests easier, use Pigweed's ``pw test`` command and pass it a path to the build
directory and the name of the test runner. By default, ``pw test`` will run all
tests, but it can be restricted it to specific ``pw_test_group`` targets using
the ``--group`` argument. Alternatively, individual test binaries can be
specified with the ``--test`` option.

.. code:: sh

  # Setup Pigweed environment.
  $ . pw_env_setup/setup.sh
  # Run test.
  $ pw test --root out/stm32f429i/ --runner stm32f429i_disc1_unit_test_runner

Run tests affected by code changes
----------------------------------
When writing code that will impact multiple modules, it's helpful to only run
all tests that are affected by a given code change. Thanks to the GN/Ninja
build, this is possible! This is done by using a ``pw_target_runner_server``
that Ninja can send the tests to as it rebuilds affected targets.

Additionally, this method enables distributed testing. If multiple devices are
connected, the tests will be run across all attached devices to further speed up
testing.

Step 1: Start test server
^^^^^^^^^^^^^^^^^^^^^^^^^
To allow Ninja to properly serialize tests to run on an arbitrary number of
devices, Ninja will send test requests to a server running in the background.
The first step is to launch this server. By default, the script will attempt
to automatically detect all attached STM32f429I-DISC1 boards and use them for
testing. To override this behavior, provide a custom server configuration file
with ``--server-config``.

.. tip::

  If you unplug or plug in any boards, you'll need to restart the test server
  for hardware changes to properly be detected.

.. code:: sh

  $ stm32f429i_disc1_test_server

Step 2: Configure GN
^^^^^^^^^^^^^^^^^^^^
By default, this hardware target has incremental testing via
``pw_target_runner`` disabled. Enabling the ``pw_use_test_server`` build arg
tells GN to send requests to a running ``stm32f429i_disc1_test_server``.

.. code:: sh

  $ gn args out/disco
  # Modify and save the args file to use pw_target_runner.
  pw_use_test_server = true

Step 3: Build changes
^^^^^^^^^^^^^^^^^^^^^
Whenever you run ``ninja -C out stm32f429i``, affected tests will be built and
run on the attached device(s). Alternatively, you may use ``pw watch`` to set up
Pigweed to build/test whenever it sees changes to source files.
