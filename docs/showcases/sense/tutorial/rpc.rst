.. _showcase-sense-tutorial-pico-rpc:

======================================
9. Communicate with your Pico over RPC
======================================
.. warning::

   (2024 August 7) This tutorial doesn't quite work yet.
   Check back tomorrow!

Now, let's revisit ``pw_console`` and ``pw_rpc``. This time, we'll send commands
to and view logs from the real Pico device.

.. _showcase-sense-tutorial-pico-rpc-interact:

----------------------
Interact with the Pico
----------------------
#. Fire up a ``pw_console`` instance that's connected to the Pico:

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         In **Bazel Build Targets** right-click
         **:rp2040_console (native_binary)** (under **//apps/blinky**)
         and then select **Run target**.

      .. tab-item:: CLI
         :sync: cli

         .. code-block:: console

            $ bazelisk run //apps/blinky:rp2040_console

#. Toggle the Pico's LED by typing the following into **Python Repl** and then
   pressing :kbd:`Enter`:

   .. code-block:: pycon

      >>> device.rpcs.blinky.Blinky.ToggleLed()

   You should see your Raspberry Pi Pico's LED turn either on or
   off. If you run the command again you should see the LED switch
   to its opposite state.

   (The next few commands should be executed the same way.)

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

#. View the RP2040's onboard temperature:

   .. code-block:: pycon

      >>> device.rpcs.board.Board.OnboardTemp()

   In **Python Results** you should see output like this:

   .. code-block:: pycon

      >>> device.rpcs.board.Board.OnboardTemp()
      (Status.OK, board.rpc.OnboardTempResponse(temp=23.861492156982422))

   Put your finger on the RP2040 chip in the middle of your Raspberry Pi
   Pico for a few seconds and then run the temperature command again and
   you should see the temperature increase.

   .. admonition:: Exercise

      Can you figure out the code to read the temperature 10 times
      with a 1-second interval between readings, and then output
      the average temperature? See
      :ref:`showcase-sense-tutorial-appendix-temp-solution` for
      one option.

#. Leave the console open and proceed to the next section.

.. _showcase-sense-tutorial-search-filter:

----------------------
Search and filter logs
----------------------
You can search and filter your device's logs. Try it now:

#. Click any row in the **Device Logs** table to focus that part of the UI.
#. Press :kbd:`/` to search the logs.
#. Type ``Stopped blinking`` and press :kbd:`Enter`. A log that matches
   that string should be highlighted.

   .. admonition:: Troubleshooting

      **No logs are shown**. There probably has just not been any
      logs that match the filter you entered. Try filtering by
      other values, such as ``00:00`` to only show logs that occurred
      during the first 60 seconds of logs.

#. Press :kbd:`n` to go to next match and :kbd:`N` to go to previous match.
   (If there are 0 matches or only 1 match then this naturally won't work.)
#. Press :kbd:`Ctrl+Alt+F` to filter out logs that don't match your query.
#. Press :kbd:`Ctrl+Alt+R` or click **Clear Filters** to clear your filter
   and return to the original table view.

.. _showcase-sense-tutorial-crash:

--------------------------------
Handle crashes (full setup only)
--------------------------------
.. warning::

   This workflow requires the :ref:`full hardware
   setup <showcase-sense-tutorial-full>`.

:ref:`module-pw_cpu_exception` provides a consistent interface
for entering a CPU exception handler and makes it easier to
consistently collect CPU state that may otherwise get clobbered
by application exception handlers. Try triggering a crash now
and downloading a snapshot:

#. In the **Python Repl** of ``pw_console`` send the following command:

   .. code-block:: pycon

      >>> device.rpcs.pw.system.proto.DeviceService.Crash()

   You should see error logs pop up in **Device Logs**.

   The traceback in **Python Output** is expected because you just
   manually triggered a crash:

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/crash_python_results.png

#. Download the crash snapshot to ``/tmp`` (or whatever directory you prefer)
   by running the following command in the **Python Repl**:

   .. code-block:: pycon

      >>> device.get_crash_snapshots('/tmp')

   In **Host Logs** you should see confirmation that a snapshot was downloaded:

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/crash_snapshot.png

#. You can leave ``pw_console`` running. We'll keep exploring it on the next
   page.

.. _showcase-sense-tutorial-pico-rpc-summary:

-------
Summary
-------
On this page we revisited our old friends ``pw_console`` and ``pw_rpc``,
except this time we used them to communicate with a real embedded
device rather than a simulated device running on our development host.
In other words, when it's time to switch from simulated devices to
real ones, you don't need to learn new tools.

We also met a new friend, ``pw_cpu_exception``, that can help us
debug crashes.

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
