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
