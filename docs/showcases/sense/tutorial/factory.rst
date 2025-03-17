.. _showcase-sense-tutorial-factory:

======================================
12. Run factory workflows at your desk
======================================
Bringing a consumer electronics product from concept to mass market
requires a lot more than just good firmware. The new hardware also
needs to be easy to test in factories. A lot of the Pigweed primitives
that you tried earlier such as :ref:`module-pw_rpc` also make
manufacturing workflows easier. Try flashing a factory app to your
Pico now and running through a Python script that exercises all of the
Enviro+ sensors.

.. important::

   This section requires a Pimoroni Enviro+ Pack. If you don't have one,
   skip ahead to :ref:`showcase-sense-tutorial-bazel_cloud`.

.. _showcase-sense-tutorial-factory-flash:

----------------------------------
Flash the factory app to your Pico
----------------------------------
#. Flash the ``factory`` binary to your Pico.

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         .. tab-set::

            .. tab-item:: Pico 1 & 1W (RP2040)

               In **Bazel Build Targets** expand **//apps/factory**, then
               right-click **:flash (native_binary)**, then select **Run
               target**.

            .. tab-item:: Pico 2 & 2W (RP2350)

               In **Bazel Build Targets** expand **//apps/factory**, then
               right-click **:flash_rp2350 (native_binary)**, then select **Run
               target**.

      .. tab-item:: CLI
         :sync: cli

         .. tab-set::

            .. tab-item:: Pico 1 & 1W (RP2040)

               .. code-block:: console

                  bazelisk run //apps/factory:flash

            .. tab-item:: Pico 2 & 2W (RP2350)

               .. code-block:: console

                  bazelisk run //apps/factory:flash_rp2350

.. _showcase-sense-tutorial-factory-tests:

--------------------
Exercise the sensors
--------------------
#. Run through the factory testing script.

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         In **Bazel Build Targets** expand **//tools**, then right-click
         **:factory (py_binary)**, then select **Run target**.

         The factory testing script launches in a VS Code terminal tab.

      .. tab-item:: CLI
         :sync: cli

         Run the factory script.

         .. code-block:: console

            $ bazelisk run //tools:factory

   .. admonition:: Troubleshooting

      ``serial.serialutil.SerialException: [Errno 16] could not open port ...
      [Errno 16] Device or resource busy``. Close the browser tab running the
      web app. The factory script couldn't connect to your Pico because the
      web app is still connected to the Pico.

#. Follow the prompts in the factory testing script. It's OK if some
   tests don't pass. This is just an example factory-at-your-desk
   workflow.

   See :ref:`showcase-sense-tutorial-factory-appendix` for an
   example of a successful walkthrough of the factory script.

   .. tip::

      One easy way to do the gas resistance test is to dip a cotton
      swab in rubbing alcohol and then hold the cotton swab close to
      the **BME688** sensor.

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/alcohol.jpg

.. _showcase-sense-tutorial-factory-summary:

-------
Summary
-------
For low-volume products a verification workflow like this may be good enough.
For high-volume products you usually need to integrate into
the manufacturer's workflows. Pigweed's abstractions, primitives, and tools often make that
easier, too. These factory-at-your-desk scripts can help you quickly prototype and iterate
on the basic workflows that will be expanded upon during the real
high-volume manufacturing process.

Next, head over to :ref:`showcase-sense-tutorial-bazel_cloud` to learn about
Bazel's cloud features.

.. _showcase-sense-tutorial-factory-appendix:

-----------------
Appendix: Example
-----------------
Here's an example of a successful walkthrough of the factory
testing workflow.

.. code-block:: text

   ===========================
   Pigweed Sense Factory Tests
   ===========================
   Operator: kayce
   Date: 2024/08/06 19:01:08
   Device flash ID: 2a4b9643086461e6

   4 tests will be performed:
     - LedTest
     - ButtonsTest
     - Ltr559Test
     - Bme688Test

   >>> Press Enter when you are ready to begin
   Starting hardware tests.

   ==========================
   [1/4] Running test LedTest
   ==========================

   >>> Is the Enviro+ LED white? [Y/n] y
   PASS: led_white

   >>> Is the Enviro+ LED red? [Y/n] y
   PASS: led_red

   >>> Is the Enviro+ LED green? [Y/n] y
   PASS: led_green

   >>> Is the Enviro+ LED blue? [Y/n] y
   PASS: led_blue

   >>> Is the Enviro+ LED off? [Y/n] y
   PASS: led_off

   ==============================
   [2/4] Running test ButtonsTest
   ==============================
   >>> Press Button A
   PASS: button_a
   >>> Press Button B
   PASS: button_b
   >>> Press Button X
   PASS: button_x
   >>> Press Button Y
   PASS: button_y

   =============================
   [3/4] Running test Ltr559Test
   =============================

   Setting LTR559 sensor to proximity mode.

   >>> Place your Enviro+ pack in a well-lit area
   Press Enter to continue...
   Getting initial sensor readings
    100.0% [==============================================================================================================>]   5/  5 eta [00:00]
    DONE
       Samples   5
       Min       0.00
       Max       0.00
       Mean      0.00


   >>> Fully cover the LIGHT sensor
   Press Enter to continue...
   Reading sensor - 28992.00, 29440.00, 29248.00, 29088.00, 29056.00
     10.0% [===========>                                                                                                   ]   5/ 50 eta [00:00]
       Samples   6
       Min       28992.00
       Max       29440.00
       Mean      29440.00
   PASS: ltr559_prox_near


   >>> Fully uncover the LIGHT sensor
   Press Enter to continue...
   Reading
     10.0% [===========>                                                                                                   ]   5/ 50 eta [00:00]
       Samples   6
       Min       0.00
       Max       0.00
       Mean      0.00
   PASS: ltr559_prox_far

   Setting LTR559 sensor to ambient mode.

   >>> Place your Enviro+ pack in an area with neutral light
   Press Enter to continue...
   Getting initial sensor readings
    100.0% [==============================================================================================================>]   5/  5 eta [00:00]
    DONE
       Samples   5
       Min       116.34lux
       Max       116.34lux
       Mean      116.34lux


   >>> Cover the LIGHT sensor with your finger
   Press Enter to continue...
   Reading - 7.34, 7.34, 7.34, 7.34, 7.34
    100.0% [==============================================================================================================>] 100/100 eta [00:00]
       Samples   100
       Min       5.39lux
       Max       11.62lux
       Mean      11.62lux
   FAIL: ltr559_light_dark

   =============================
   [4/4] Running test Bme688Test
   =============================

   Testing gas resistance in the BME688 sensor.
   To test the BME688's gas sensor, you need an alcohol-based
   solution. E.g. dip a cotton swab in rubbing alcohol.

   >>> Are you able to continue this test? [Y/n] y
   Getting initial sensor readings - 5684.85, 5684.85, 5684.85, 5684.85, 5684.85
    100.0% [==============================================================================================================>]  10/ 10 eta [00:00]
    DONE
       Samples   10
       Min       5684.85
       Max       1173639.00
       Mean      1173639.00

   >>> Move the alcohol close to the BME688 sensor.
   Press Enter to begin measuring...
   Reading sensor - 5684.85, 5684.85, 5684.85, 5684.85, 5684.85
     10.0% [===========>                                                                                                   ]   5/ 50 eta [00:00]
       Samples   6
       Min       5684.85
       Max       5684.85
       Mean      5684.85
   PASS: bme688_gas_resistance_poor

   >>> Move the alcohol away from the BME688 sensor
   Press Enter to continue...
   Reading sensor - 30468.94, 31067.96, 31573.75, 31928.16, 32290.62
    100.0% [==============================================================================================================>]  50/ 50 eta [00:00]
       Samples   50
       Min       5684.85
       Max       32290.62
       Mean      32290.62
   FAIL: bme688_gas_resistance_normal

   Testing BME688's temperature sensor.
   Getting initial sensor readings - 28.11, 28.11, 28.12, 28.12, 28.11
    100.0% [==============================================================================================================>]  10/ 10 eta [00:00]
    DONE
       Samples   10
       Min       27.86C
       Max       28.12C
       Mean      28.12C

   >>> Put your finger on the BME688 sensor to increase its temperature
   Press Enter to begin measuring...
   Reading sensor - 30.62, 30.53, 31.27, 31.78, 32.02
     88.0% [================================================================================================>              ]  44/ 50 eta [00:00]
       Samples   45
       Min       29.70C
       Max       32.02C
       Mean      32.02C
   PASS: bme688_temperature_hot

   >>> Remove your finger from the BME688 sensor
   Press Enter to begin measuring...
   Reading sensor - 29.86, 30.11, 30.05, 29.99, 29.93
      5.0% [=====>                                                                                                         ]   5/100 eta [00:00]
       Samples   6
       Min       29.81C
       Max       30.11C
       Mean      30.11C
   PASS: bme688_temperature_normal

   ============
   Test Summary
   ============
   Operator: kayce
   Date: 2024/08/06 19:01:08
   Device flash ID: 2a4b9643086461e6

   LedTest
     PASS | led_white
     PASS | led_red
     PASS | led_green
     PASS | led_blue
     PASS | led_off

   ButtonsTest
     PASS | button_a
     PASS | button_b
     PASS | button_x
     PASS | button_y

   Ltr559Test
     PASS | ltr559_prox_near
     PASS | ltr559_prox_far
     FAIL | ltr559_light_dark

   Bme688Test
     PASS | bme688_gas_resistance_poor
     FAIL | bme688_gas_resistance_normal
     PASS | bme688_temperature_hot
     PASS | bme688_temperature_normal

   14 tests passed, 2 tests failed.
   ========================================
   Device logs written to /home/kayce/tmp/cli/sense/factory-logs-20240806190108-device.txt
   Factory logs written to /home/kayce/tmp/cli/sense/factory-logs-20240806190108-operator.txt
