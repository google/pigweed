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
To build for this target, change the ``pw_target_config`` GN build arg to point
to this target's configuration file.

.. code:: sh

  $ gn gen --args='pw_target_config = "//targets/stm32f429i-disc1/target_config.gni"' out/disco
  $ ninja -C out/disco

or

.. code:: sh

  $ gn gen out/disco
  $ gn args
  # Modify and save the args file to update the pw_target_config.
  pw_target_config = "//targets/stm32f429i-disc1/target_config.gni"
  $ ninja -C out/disco

Testing
=======
The default Pigweed build target will build all of the pigweed module unit
tests. These tests can be run on-device in a few different ways.

Run a unit test
---------------
Test if using ``out/disco`` as your build directory, tests will be located in
``out/disco/obj/[module name]/[test_name].elf``. To run these on device, the
stm32f429i-disc1 target provides a helper script that flashes the test to a
device and then runs it.

.. code:: sh

  # Setup pigweed environment.
  $ . env_setup/setup.sh
  # Run test.
  $ stm32f429i_disc1_unit_test_runner /path/to/binary

Run multiple tests
------------------
Running all tests one-by-one is rather tedious. To make running multiple
tests easier, use Pigweed's ``pw test`` command and pass it your build directory
and the name of the test runner. By default, ``pw test`` will run all tests,
but you can restrict it to specific groups using the ``--group`` flag.
Individual test binaries can be specified with the ``--test`` flag as well.

.. code:: sh

  # Setup pigweed environment.
  $ . env_setup/setup.sh
  # Run test.
  $ pw test --root out/disco/ --runner stm32f429i_disc1_unit_test_runner

Run affected tests (EXPERIMENTAL)
---------------------------------
When writing code that will impact multiple modules, it's helpful to only run
all tests that are affected by a given code change. Thanks to the GN/ninja
build, this is possible! This is done by using a pw_target_runner_server that
ninja can send the tests to as it rebuilds affected targets.

Additionally, this method enables distributed testing. If you connect multiple
devices, the tests will be run across the attached devices to further speed up
testing.


.. warning::

  This requires pw_target_runner_server and pw_target_runner_client have been
  built and are in your PATH. By default, you can find these binaries in the
  `host_tools` directory of a host build (e.g. out/host/host_tools).

Step 1: Start test server
^^^^^^^^^^^^^^^^^^^^^^^^^
To allow ninja to properly serialize tests to run on an arbitrary number of
devices, ninja will send test requests to a server running in the background.
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
By default, this hardware target has incremental testing via pw_test_server
disabled. Enabling this build arg tells GN to send requests to

.. code:: sh

  $ gn args out/disco
  # Modify and save the args file to use pw_test_server.
  pw_use_test_server = true

Step 3: Build changes
^^^^^^^^^^^^^^^^^^^^^
Whenever you run ``ninja -C out/disco``, affected tests will be built and run on
the attached device(s). Alternatively, you may use ``pw watch`` to set up
Pigweed to build/test whenever it sees changes to source files.
