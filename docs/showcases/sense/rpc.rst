.. _showcase-sense-tutorial-pico-rpc:

======================================
9. Communicate with your Pico over RPC
======================================
Now, let's revisit ``pw_console`` and ``pw_rpc``. This time, we'll send commands
to and view logs from the real Pico device.

.. warning::

   https://pwrev.dev/405441939 - The LED does not work yet on the Pico W or
   Pico 2 W.


.. _showcase-sense-tutorial-pico-rpc-interact:

----------------------
Interact with the Pico
----------------------
#. Fire up a ``pw_console`` instance that's connected to the Pico:

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         .. tab-set::

            .. tab-item:: Pico 1 & 1W (RP2040)
               :sync: rp2040

               In **Bazel Build Targets** right-click
               **:rp2040_console (native_binary)** (under **//apps/blinky**)
               and then select **Run target**.

            .. tab-item:: Pico 2 & 2W (RP2350)
               :sync: rp2350

               In **Bazel Build Targets** right-click
               **:rp2350_console (native_binary)** (under **//apps/blinky**)
               and then select **Run target**.

      .. tab-item:: CLI
         :sync: cli

         .. tab-set::

            .. tab-item:: Pico 1 & 1W (RP2040)
               :sync: rp2040

               .. code-block:: console

                  bazelisk run //apps/blinky:rp2040_console

            .. tab-item:: Pico 2 & 2W (RP2350)
               :sync: rp2350

               .. code-block:: console

                  bazelisk run //apps/blinky:rp2350_console

         If you see a selection prompt like the one below, select
         **Debug Probe (CMSIS-DAP) - CDC-ACM UART Interface**.

         .. code-block:: text

            Please select a serial port device.
            Available ports:
              1 - /dev/ttyACM0 - Raspberry Pi - Debug Probe (CMSIS-DAP) - CDC-ACM UART Interface
              2 - /dev/ttyACM1 - Raspberry Pi - Pico - Board CDC
              3 - /dev/ttyS0 - None - n/a
              4 - /dev/ttyS1 - None - n/a
              5 - /dev/ttyS2 - None - n/a
              6 - /dev/ttyS3 - None - n/a

#. Toggle the Pico's LED by typing the following into **Python Repl** and then
   pressing :kbd:`Enter`:

   .. code-block:: pycon

      >>> device.rpcs.blinky.Blinky.ToggleLed()

   You should see your Raspberry Pi Pico's LED turn either on or
   off. If you run the command again you should see the LED switch
   to its opposite state.

   (The next few commands should also be executed in the
   **Python Repl**, just like the last one.)

#. Blink the LED 10 times:

   .. code-block:: pycon

      >>> device.rpcs.blinky.Blinky.Blink(interval_ms=1000, blink_count=10)

#. Write some custom automation in the Python REPL to achieve the same
   blinking effect:

   .. code-block:: pycon

      >>> def my_blinky(count, delay):
      ...     from time import sleep
      ...     toggle = device.rpcs.blinky.Blinky.ToggleLed
      ...     for _ in range(count):
      ...         toggle()
      ...         sleep(delay)
      ...
      >>> my_blinky(20, 1)


   .. note::

      The REPL doesn't currently support top-level execution of multiple
      statements. You can workaround this by wrapping your multi-statement
      logic in a function and then invoking the function, as seen in
      ``my_blinky()``.

#. View your board's onboard temperature:

   .. code-block:: pycon

      >>> device.rpcs.board.Board.OnboardTemp()

   In **Python Results** (top-left pane) you should see output like this:

   .. code-block:: pycon

      >>> device.rpcs.board.Board.OnboardTemp()
      (Status.OK, board.rpc.OnboardTempResponse(temp=23.861492156982422))

   Put your finger on the microprocessor in the middle of your Pico (the black
   square with a raspberry logo etched on it) for a few seconds and then run
   the temperature command again and you should see the temperature increase.

   .. admonition:: Exercise

      Can you figure out the code to read the temperature 10 times
      with a 1-second interval between readings, and then output
      the average temperature? See
      :ref:`showcase-sense-tutorial-appendix-temp-solution` for
      one solution.

#. Leave the console open and proceed to the next section.

.. _showcase-sense-tutorial-search-filter:

----------------------
Search and filter logs
----------------------
You can search and filter your device's logs. Try it now:

#. Click anywhere in the **Device Logs** table (top-right pane) to focus that part of the UI.
#. Press :kbd:`/` to search the logs.
#. Type ``ON`` and press :kbd:`Enter`. A log that matches
   that string should be highlighted.
#. Press :kbd:`n` to go to next match and :kbd:`N` to go to previous match.
#. Press :kbd:`Ctrl+Alt+F` to filter out logs that don't match your query.
#. Press :kbd:`Ctrl+Alt+R` or click **Clear Filters** to clear your filter
   and return to the original table view.

-----------------------
Keep pw_console running
-----------------------
There's no need to close ``pw_console`` right now. You're going to use it
on the next page.

.. _showcase-sense-tutorial-pico-rpc-summary:

-------
Summary
-------
On this page we revisited our old friends ``pw_console`` and ``pw_rpc``,
except this time we used them to communicate with a real embedded
device rather than a simulated device running on our development host.
In other words, when it's time to switch from simulated devices to
real ones, you don't need to learn new tools.

Next, head over to :ref:`showcase-sense-tutorial-automate` to
learn how to package up common development tasks into small scripts
so that your whole team can benefit from them.

--------
Appendix
--------

.. _showcase-sense-tutorial-appendix-temp-solution:

Temperature averaging solution
==============================
Here's one possible solution to the temperature averaging exercise
at the bottom of :ref:`showcase-sense-tutorial-pico-rpc-interact`.

.. code-block:: py

   def average(count, delay):
       from time import sleep
       total = 0
       sample = device.rpcs.board.Board.OnboardTemp
       for _ in range(count):
           status, data = sample()
           total += data.temp
           sleep(delay)
       return total / count

   average(10, 1)
