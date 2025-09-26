.. _module-pw_async2-codelab-step5:

============================
5. Communicate between tasks
============================
.. pigweed-module-subpage::
   :name: pw_async2

Your vending machine is ready to handle more functionality. In this step,
you'll write code to handle a dispenser mechanism. The vending machine uses a
motor to push the selected product into a chute. A sensor detects when the item
has dropped, then the motor is turned off.

The dispenser mechanism is complex enough to merit a task of its own. The
``VendingMachineTask`` will notify the new ``DispenserTask`` about which items
to dispense, and ``DispenserTask`` will send confirmation back.

---------------------------
Set up the item drop sensor
---------------------------
The ``ItemDropSensor`` class that we've provided is similar to the
``CoinSlot`` and ``Keypad`` classes.

#. Integrate the item drop sensor into ``main.cc``:

   * Include ``item_drop_sensor.h``

   * Create a global ``codelab::ItemDropSensor item_drop_sensor`` instance

   * Call the item drop sensor's ``Drop()`` method in the interrupt handler
     (``item_drop_sensor_isr()``)

   When an item is dispensed successfully, the item drop sensor triggers an
   interrupt, which is handled by the ``item_drop_sensor_isr()`` function.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/main.cc
         :language: cpp
         :linenos:
         :start-after: // the License.
         :end-before: int main() {
         :emphasize-lines: 3,10,26

-----------------------------
Set up communication channels
-----------------------------
We'll be adding a new ``DispenserTask`` soon. To get ready for that, let's set
up communications channels between ``VendingMachineTask`` and the new task.

There are many ways to use a :cc:`Waker <pw::async2::Waker>` to
communicate between tasks. For this step, we'll use a one-deep
:cc:`pw::InlineAsyncQueue` to send events between the two tasks.

#. Set up the queues in ``vending_machine.h``:

   * Include ``pw_containers/inline_async_queue.h``

   * Create a ``DispenseRequestQueue`` alias for ``pw::InlineAsyncQueue<int, 1>``
     and a ``DispenseResponseQueue`` alias for ``pw::InlineAsyncQueue<bool, 1>``

   You'll use ``DispenseRequestQueue`` to send dispense requests (item numbers)
   from ``VendingMachineTask`` to ``DispenserTask``. ``DispenseResponseQueue``
   is for sending dispense responses (success or failure) from ``DispenserTask``
   to ``VendingMachineTask``.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-after: #pragma once
         :end-at: using DispenseResponseQueue
         :linenos:
         :emphasize-lines: 9,15,16

#. Back in ``main.cc``, set up the queues:

   * Declare a ``dispense_requests`` queue and a ``dispense_response`` queue

   * Provide the queues to ``VendingMachineTask`` when the task is created

   * Create a ``DispenserTask`` instance (we'll implement this in the next
     section)

   * Post the new ``DispenserTask`` to the dispatcher

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/main.cc
         :language: cpp
         :linenos:
         :start-at: int main() {
         :emphasize-lines: 5,6,9,10,12-14

.. topic:: ``pw::InlineAsyncQueue`` async member functions

   :cc:`pw::InlineAsyncQueue` adds two async member functions to
   :cc:`pw::InlineQueue`:

   * :cc:`PendHasSpace() <pw::BasicInlineAsyncQueue::PendHasSpace>`:
     Producers call this to ensure the queue has room before producing more
     data.

     .. code-block:: c++

        PW_TRY_READY(async_queue.PendHasSpace(context));
        async_queue.push_back(item);

   * :cc:`PendNotEmpty() <pw::BasicInlineAsyncQueue::PendNotEmpty>`:
     Consumers call this to block until data is available to consume.

     .. code-block:: c++

        PW_TRY_READY(async_queue.PendNotEmpty(context));
        Item& item = async_queue.front();
        async_queue.pop();  // Remove the item when done with it.

-----------------------------
Create the new dispenser task
-----------------------------
The ``DispenserTask`` will turn the dispenser motor on and off in response to
dispense requests from the ``VendingMachineTask``.

#. Declare a new ``DispenserTask`` class in ``vending_machine.h``:

   * The ``DispenserTask`` constructor should accept references to the drop
     sensor and both comms queues as args

   * Create a ``State`` enum member with these states:

     * ``kIdle``: Waiting for a dispense request (motor is off)

     * ``kDispensing``: Actively dispensing an item (motor is on)

     * ``kReportDispenseSuccess``: Waiting to report success (motor is off)

   * Create a data member to store the current state

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-at: class DispenserTask
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 1-25

#. Implement the dispenser's state machine in ``vending_machine.cc``:

   * Handle the ``kIdle``, ``kDispensing``, and ``kReportDispenseSuccess``
     states (as well as the transitions between them)

   * Use the ``SetDispenserMotorState`` function that's provided in
     ``hardware.h`` to control the dispenser's motor

   .. note::

      Dispensing can't fail yet. We'll get to that later.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<> DispenserTask::DoPend
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 1-41

-------------------------
Communicate between tasks
-------------------------
Now, let's get ``VendingMachineTask`` communicating with ``DispenserTask``.

Instead of just logging when a purchase is made, ``VendingMachineTask`` will
send the selected item to the ``DispenserTask`` through the dispense requests
queue. Then it will wait for a response with the dispense responses queue.

#. Prepare ``VendingMachineTask`` for comms in ``vending_machine.h``:

   * Add the communication queues as parameters to the ``VendingMachineTask``
     constructor

   * Add new states: ``kAwaitingDispenseIdle`` (dispenser is ready for a request)
     and ``kAwaitingDispense`` (waiting for dispenser to finish dispensing an
     item)

   * Add data members for storing the communication queues

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-at: class VendingMachineTask
         :end-before: class DispenserTask
         :linenos:
         :emphasize-lines: 5-6,12-13,38-39,45-46

#. Update the vending machine task's state machine in ``vending_machine.cc``:

   * Transition the ``kAwaitingSelection`` state to ``kAwaitingDispenseIdle``

   * Implement the ``kAwaitingDispenseIdle`` and ``kAwaitingDispense`` states

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-at: case kAwaitingSelection: {
         :end-before: pw::async2::Poll<> DispenserTask::DoPend
         :linenos:
         :emphasize-lines: 18,26-48

------------------
Test the dispenser
------------------
#. Run the app:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Press :kbd:`c` :kbd:`Enter` to input a coin.

#. Press :kbd:`1` :kbd:`Enter` to make a selection.

#. Press :kbd:`i` :kbd:`Enter` to trigger the item drop sensor, signaling that
   the item has finished dispensing.

   You should see the vending machine display it's welcome message again.

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      INF  Dispenser task awake
      c
      INF  Received 1 coin.
      INF  Please press a keypad key.
      1
      INF  Keypad 1 was pressed. Dispensing an item.
      INF  Dispenser task awake
      INF  [Motor for item 1 set to On]
      i
      INF  Dispenser task awake
      INF  [Motor for item 1 set to Off]
      INF  Dispense succeeded. Thanks for your purchase!
      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.

Congratulations! You now have a fully functioning vending machine! Or do you…?

------------------------------------------
Handle unexpected situations with timeouts
------------------------------------------
What if you press the wrong button and accidentally buy an out-of-stock item?
As of now, the dispenser will just keep running forever. The vending
machine will eat your money while you go hungry.

Let's fix this. We can add a timeout to the ``kDispensing`` state. If the
``ItemDropSensor`` hasn't triggered after a certain amount of time, then
something has gone wrong. The ``DispenserTask`` should stop the motor and tell
the ``VendingMachineTask`` what happened. You can implement a timeout with
:cc:`TimeFuture <pw::async2::TimeFuture>`.

#. Prepare ``DispenserTask`` to support timeouts in ``vending_machine.h``:

   * Include the headers that provide timeout-related features:

     * ``pw_async2/system_time_provider.h``

     * ``pw_async2/time_provider.h``

     * ``pw_chrono/system_clock.h``

     .. dropdown:: Hint

        .. literalinclude:: ./checkpoint2/vending_machine.h
           :language: cpp
           :start-after: // the License.
           :end-before: namespace codelab {
           :linenos:
           :emphasize-lines: 9,11,13

   * Create a new ``kReportDispenseFailure`` state to represent dispense
     failures, a new ``kDispenseTimeout`` data member in ``DispenserTask`` that
     holds the timeout duration (``std::chrono::seconds(5)`` is a good value),
     and a ``pw::async2::TimeFuture<pw::chrono::SystemClock> timeout_future_``
     member for holding the timeout future:

     .. dropdown:: Hint

        .. literalinclude:: ./checkpoint2/vending_machine.h
           :language: cpp
           :start-at: class DispenserTask
           :end-before: }  // namespace codelab
           :linenos:
           :emphasize-lines: 17,27,28

     For testing purposes, make sure that the timeout period is long enough for
     a human to respond.

#. Implement the timeout support in ``vending_machine.cc``:

   * When you start dispensing an item (in your transition from ``kIdle`` to
     ``kDispensing``), initialize the :cc:`TimeFuture <pw::async2::TimeFuture>`
     to your timeout.

   * In the ``kDispensing`` state, use :cc:`Select <pw::async2::Select>`
     to wait for either the timeout or the item drop signal, whichever comes first.

   * Use :cc:`VisitSelectResult <pw::async2::VisitSelectResult>` to take action
     based on the result:

     * If the item drop interrupt arrives first, clear the timeout with
       ``timeout_future_ = {}``. If the timer isn't cleared, it will fire later and
       wake ``DispenserTask`` unnecessarily, wasting time and power. After that,
       proceed to the ``kReportDispenseSuccess`` state.

     * If the timeout arrives first, proceed to the ``kReportDispenseFailure`` state.

     * In either case, be sure to turn off the motor and ``pop()`` the dispense request
       from the queue.

   * Handle the dispense failure state.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint2/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<> DispenserTask::DoPend
         :end-at: }  // namespace codelab
         :linenos:
         :emphasize-lines: 15-18,24-32,39-50,62-66

--------------------------------
Test the dispenser with timeouts
--------------------------------
#. Run the app:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Press :kbd:`c` :kbd:`Enter` to input a coin.

# Press :kbd:`1` :kbd:`Enter` to make a selection.

#. Wait for the timeout.

   After 5 seconds you should see a message about the dispense failing.

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      INF  Dispenser task awake
      c
      INF  Received 1 coin.
      INF  Please press a keypad key.
      1
      INF  Keypad 1 was pressed. Dispensing an item.
      INF  [Motor for item 1 set to On]
      INF  [Motor for item 1 set to Off]
      INF  Dispense failed. Choose another selection.

#. Try again, but this time press :kbd:`i` :kbd:`Enter` in 5 seconds or less
   so that the dispense succeeds.

----------
Next steps
----------
Congratulations! You completed the codelab. Click **DISPENSE PRIZE** to
retrieve your prize.

.. raw:: html

   <div style="margin-bottom:1em">
     <button id="dispense">DISPENSE PRIZE</button>
     <p id="prize" style="display:none">🍭</p>
   </div>
   <script>
     document.querySelector("#dispense").addEventListener("click", (e) => {
       e.target.disabled = true;
       document.querySelector("#prize").style.display = "block";
     });
   </script>

You now have a solid foundation in ``pw_async2`` concepts, and quite a bit
of hands-on experience with the framework. Try building something yourself
with ``pw_async2``! As always, if you get stuck, or if anything is unclear,
or you just want to run some ideas by us, :ref:`we would be happy to chat
<module-pw_async2-codelab-help>`.

.. _module-pw_async2-codelab-step5-checkpoint:

----------
Checkpoint
----------
At this point, your code should look similar to the files below.

.. tab-set::

   .. tab-item:: main.cc

      .. literalinclude:: ./checkpoint2/main.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.cc

      .. literalinclude:: ./checkpoint2/vending_machine.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.h

      .. literalinclude:: ./checkpoint2/vending_machine.h
         :language: cpp
         :start-after: // the License.
