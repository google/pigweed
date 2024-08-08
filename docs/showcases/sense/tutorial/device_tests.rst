.. _showcase-sense-tutorial-devicetests:

======================
8. Run on-device tests
======================
.. note::

   This step requires the :ref:`full hardware
   setup <showcase-sense-tutorial-full>` and must be
   run over a terminal.

Pigweed is all about helping big teams scale, so of course we built out
a robust and automated solution for running tests on-device! Try it now:

.. _showcase-sense-tutorial-devicetests-setup:

--------------------
Set up your hardware
--------------------
#. Set up your hardware as described in :ref:`showcase-sense-tutorial-full`.
   On-device tests don't work with the basic setup. You can skip ahead to
   :ref:`showcase-sense-tutorial-pico-rpc` if you don't have the full setup
   hardware.

.. _showcase-sense-tutorial-devicetests-run:

-------------------
Run on-device tests
-------------------
.. _Command Palette: https://code.visualstudio.com/docs/getstarted/userinterface#_command-palette
.. _terminal: https://code.visualstudio.com/docs/terminal/basics

.. tab-set::

   .. tab-item:: VS Code
      :sync: vsc

      #. Open the `Command Palette`_.

      #. Run the ``Pigweed: Set Bazelisk Path`` command and select
         **Use the version built into the Pigweed extension**.

      #. Open the Command Palette again and run the
         ``Pigweed: Activate Bazelisk In Terminal`` command.

      #. Open a VS Code `terminal`_.

         .. note::

            This section requires the VS Code terminal because there's currently
            no way to run wildcard tests from the **Bazel Build Targets** UI.
            Wildcard tests will be explained more in a moment.

      #. Start the test runner:

         .. code-block:: console

            $ bazelisk run \
                @pigweed//targets/rp2040/py:unit_test_server \
                -- --debug-probe-only
            INFO: Analyzed target @@pigweed~//targets/rp2040/py:unit_test_server (134 packages loaded, 13872 targets configured).
            INFO: Found 1 target...
            Target @@pigweed~//targets/rp2040/py:unit_test_server up-to-date:
              bazel-bin/external/pigweed~/targets/rp2040/py/unit_test_server
            INFO: Elapsed time: 32.497s, Critical Path: 18.71s
            INFO: 177 processes: 12 internal, 165 linux-sandbox.
            INFO: Build completed successfully, 177 total actions
            INFO: Running command line: bazel-bin/external/pigweed~/targets/rp2040/py/unit_test_server <args omitted>
            20240806 18:22:29 OUT [370633] 2024/08/06 18:22:29 Parsed server configuration from /tmp/tmparhr7i8o
            20240806 18:22:29 OUT [370633] 2024/08/06 18:22:29 Registered ExecDeviceRunner /home/kayce/.cache/bazel/_bazel_kayce/12747149b267f61f52f2c26162a31942/execroot/_main/bazel-out/k8-fastbuild/bin/external/pigweed~/targets/rp2040/py/rpc_unit_test_runner with args [--usb-bus 3 --usb-port 6]
            20240806 18:22:29 OUT [370633] 2024/08/06 18:22:29 Starting gRPC server on [::]:34172
            20240806 18:22:29 OUT [370633] [ServerWorkerPool] 2024/08/06 18:22:29 Starting 1 workers
            20240806 18:22:29 OUT [370633] [ExecDeviceRunner 0] 2024/08/06 18:22:29 Starting worker

         The test runner essentially orchestrates how all the unit
         tests should be run.

         .. note::

            You don't need to manually install ``bazelisk``. The Pigweed extension
            put its bundled version of ``bazelisk`` on your ``$PATH`` when
            you ran the ``Pigweed: Set Bazelisk Path`` command.

      #. Open another terminal and run the tests:

         .. code-block:: console

            $ bazelisk test --config=rp2040 //...
            # ...
            INFO: Found 134 targets and 10 test targets...
            INFO: Elapsed time: 131.231s, Critical Path: 60.93s
            INFO: 2368 processes: 423 internal, 1945 linux-sandbox.
            INFO: Build completed successfully, 2368 total actions
            //modules/pubsub:service_test                                  SKIPPED
            //modules/air_sensor:air_sensor_test                           PASSED in 30.2s
            //modules/blinky:blinky_test                                   PASSED in 14.3s
            //modules/buttons:manager_test                                 PASSED in 41.0s
            //modules/edge_detector:hysteresis_edge_detector_test          PASSED in 7.3s
            //modules/lerp:lerp_test                                       PASSED in 26.0s
            //modules/morse_code:encoder_test                              PASSED in 35.6s
            //modules/pubsub:pubsub_events_test                            PASSED in 18.2s
            //modules/pubsub:pubsub_test                                   PASSED in 22.1s
            //modules/state_manager:state_manager_test                     PASSED in 38.5s

            Executed 9 out of 10 tests: 9 tests pass and 1 was skipped.

      #. Go to the terminal that the test runner is running in and press
         :kbd:`Control+C` to close it.

   .. tab-item:: CLI
      :sync: cli

      #. Open a terminal and start a test runner:

         .. code-block:: console

            $ bazelisk run \
                @pigweed//targets/rp2040/py:unit_test_server \
                -- --debug-probe-only
            INFO: Analyzed target @@pigweed~//targets/rp2040/py:unit_test_server (134 packages loaded, 13872 targets configured).
            INFO: Found 1 target...
            Target @@pigweed~//targets/rp2040/py:unit_test_server up-to-date:
              bazel-bin/external/pigweed~/targets/rp2040/py/unit_test_server
            INFO: Elapsed time: 32.497s, Critical Path: 18.71s
            INFO: 177 processes: 12 internal, 165 linux-sandbox.
            INFO: Build completed successfully, 177 total actions
            INFO: Running command line: bazel-bin/external/pigweed~/targets/rp2040/py/unit_test_server <args omitted>
            20240806 18:22:29 OUT [370633] 2024/08/06 18:22:29 Parsed server configuration from /tmp/tmparhr7i8o
            20240806 18:22:29 OUT [370633] 2024/08/06 18:22:29 Registered ExecDeviceRunner /home/kayce/.cache/bazel/_bazel_kayce/12747149b267f61f52f2c26162a31942/execroot/_main/bazel-out/k8-fastbuild/bin/external/pigweed~/targets/rp2040/py/rpc_unit_test_runner with args [--usb-bus 3 --usb-port 6]
            20240806 18:22:29 OUT [370633] 2024/08/06 18:22:29 Starting gRPC server on [::]:34172
            20240806 18:22:29 OUT [370633] [ServerWorkerPool] 2024/08/06 18:22:29 Starting 1 workers
            20240806 18:22:29 OUT [370633] [ExecDeviceRunner 0] 2024/08/06 18:22:29 Starting worker

         The test runner essentially orchestrates how all the unit
         tests should be run.

      #. Open another terminal and run the tests:

         .. code-block:: console

            $ bazelisk test --config=rp2040 //...
            # ...
            INFO: Found 134 targets and 10 test targets...
            INFO: Elapsed time: 131.231s, Critical Path: 60.93s
            INFO: 2368 processes: 423 internal, 1945 linux-sandbox.
            INFO: Build completed successfully, 2368 total actions
            //modules/pubsub:service_test                                  SKIPPED
            //modules/air_sensor:air_sensor_test                           PASSED in 30.2s
            //modules/blinky:blinky_test                                   PASSED in 14.3s
            //modules/buttons:manager_test                                 PASSED in 41.0s
            //modules/edge_detector:hysteresis_edge_detector_test          PASSED in 7.3s
            //modules/lerp:lerp_test                                       PASSED in 26.0s
            //modules/morse_code:encoder_test                              PASSED in 35.6s
            //modules/pubsub:pubsub_events_test                            PASSED in 18.2s
            //modules/pubsub:pubsub_test                                   PASSED in 22.1s
            //modules/state_manager:state_manager_test                     PASSED in 38.5s

            Executed 9 out of 10 tests: 9 tests pass and 1 was skipped.

      #. Go to the terminal that the test runner is running in and press
         :kbd:`Control+C` to close it.

.. note::

   The ``//...`` in this command is what makes this a wildcard
   test. ``//...`` means "run all of the on-device tests defined
   thoughout the project".

.. _showcase-sense-tutorial-devicetests-flash:

-----------------------------------------
Flash the blinky app onto your Pico again
-----------------------------------------
You're done with the on-device tests. Flash the ``blinky`` app back onto
your Pico again:

.. tab-set::

   .. tab-item:: VS Code
      :sync: vsc

      In **Bazel Build Targets** expand **//apps/blinky**, then right-click
      **:flash (alias)**, then select **Run target**.

   .. tab-item:: CLI
      :sync: cli

      .. code-block:: console

         $ bazelisk run //apps/blinky:flash

.. _showcase-sense-tutorial-devicetests-summary:

-------
Summary
-------
:ref:`Host tests <showcase-sense-tutorial-hosttests>` are a great way to
verify that hardware-agnostic application logic is correct. For any logic
that's intimately connected to hardware, however, on-device tests are the
way to go.

Next, head over to :ref:`showcase-sense-tutorial-pico-rpc` to
explore how to communicate with your Pico from your host.
