.. _module-pw_async2-codelab-step3:

======================================
3. Create and wake a pendable function
======================================
.. pigweed-module-subpage::
   :name: pw_async2

Currently, your vending machine immediately dispenses an item after receiving
coins. In this step, you modify your app to let customers choose what to buy.
Along the way you will learn how to correctly wake up a task that is waiting for
input like this. You will also gain some experience implementing a ``Pend()``
function yourself.

---------------------
Stub the Keypad class
---------------------
The hardware simulator sends you keypad events via async calls to
``key_press_isr()``. The simulator sends integer values between 0 and 9
that indicate which keypad button was pressed. Your mission is to process
these keypad events safely and to allow your task to wait for keypad
numbers after receiving coins.

#. Use the keypad in ``main.cc``:

   * Declare a global ``keypad`` instance

   * Update ``key_press_isr()`` to handle keypad input events by invoking
     ``keypad.Press(key)``

   * Pass a reference to the keypad when creating your task instance

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/main.cc
         :language: cpp
         :start-at: namespace {
         :linenos:
         :emphasize-lines: 4,14,29

#. Set up the keypad class in ``vending_machine.h`` (as well as a data member
   for tracking how many coins have been inserted):

   * Declare the stub ``Keypad`` class:

     .. literalinclude:: ./checkpoint1/vending_machine.h
        :language: cpp
        :start-at: class Keypad {
        :end-at: };
        :linenos:

   * Set up a private ``keypad_`` member for ``VendingMachineTask``

   * Add a ``Keypad&`` argument to the ``VendingMachineTask`` constructor
     parameters and use it to initialize the ``keypad_`` member

   * Set up an ``unsigned coins_inserted_`` data member in
     ``VendingMachineTask`` that tracks how many coins have been inserted and
     initialize ``coins_inserted_`` to ``0`` in the constructor

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-at: namespace codelab {
         :end-at: }  // namespace codelab
         :linenos:
         :emphasize-lines: 3-21,26,30,31,40,41

#. Create a stub implementation and integrate it (as well
   as the coin count data member) in ``vending_machine.cc``:

   * Create a stub ``Keypad`` implementation:

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<int> Keypad::Pend
         :end-at: void Keypad::Press(int key) {}
         :linenos:
         :emphasize-lines: 1-3,5

      Notice how the ``Pend`` member function just immediately returns the value
      of ``key_pressed_``, which is only ever set to ``kNone`` (``-1``). We will
      fix that later.

   * Update ``VendingMachineTask::DoPend()`` to check if coins have already
     been inserted before it pends for coins (``coin_slot_.Pend(cx)``)

   * Wait for keypad input by invoking ``keypad_.Pend(cx)``) after coins have
     been inserted

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-after: namespace codelab {
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 1-3,5,14-19,21

------------------------------
Verify the stub implementation
------------------------------
#. Run the app:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Press :kbd:`c` :kbd:`Enter` to insert a coin.

   You should see a log stating that ``-1`` was pressed. This is expected since
   the ``KeyPad::Pend()`` stub implementation returns ``key_pressed_``, which
   was initialized to ``kNone`` (``-1``).

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      c
      INF  Received 1 coin.
      INF  Please press a keypad key.
      INF  Keypad -1 was pressed. Dispensing an item.

--------------------
Handle keypad events
--------------------
Now, let's update the ``Keypad`` implementation to actually handle key
presses.

#. Protect keypad data access in ``vending_machine.h``:

   * Include the ``pw_sync/interrupt_spin_lock.h`` and
     ``pw_sync/lock_annotations.h`` headers

   * Add a ``pw::sync::InterruptSpinLock lock_`` private member to ``Keypad``

   * Guard ``key_pressed_`` with the spin lock with :cc:`PW_GUARDED_BY`

   Since the keypad ISR is asynchronous, you'll need to synchronize access to
   the stored event data. For this codelab, we use
   :cc:`pw::sync::InterruptSpinLock` which is safe to acquire from an ISR in
   production use. Alternatively you can use atomic operations.

   We use :cc:`PW_GUARDED_BY` to add a compile-time check to ensure that
   the protected key press data is only accessed when the spin lock is held.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint2/vending_machine.h
         :language: cpp
         :start-after: #pragma once
         :end-before: // The main task that drives the vending machine.
         :linenos:
         :emphasize-lines: 5,6,27-28

#. Implement keypress handling in ``vending_machine.cc``:

   * In ``Keypad::Pend()``, attempt to read the keypress data

   * In ``Keypad::Press()``, store the keypress data

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint2/vending_machine.cc
         :language: cpp
         :start-after: namespace codelab {
         :end-before: pw::async2::Poll<> VendingMachineTask::DoPend
         :linenos:
         :emphasize-lines: 3-8,11-14

      ``std::exchange`` ensures that the ``key_pressed_`` data is read once by
      clearing it out to ``kNone`` (``-1``) after a read.

It's so simple… what could go wrong?

------------------------------
Test the keypad implementation
------------------------------
#. Run the app:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Press :kbd:`c` :kbd:`Enter` to insert a coin.

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      c
      INF  Received 1 coin.
      INF  Please press a keypad key.

         ▄████▄      ██▀███      ▄▄▄           ██████     ██░ ██
        ▒██▀ ▀█     ▓██ ▒ ██▒   ▒████▄       ▒██    ▒    ▓██░ ██▒
        ▒▓█ 💥 ▄    ▓██ ░▄█ ▒   ▒██  ▀█▄     ░ ▓██▄      ▒██▀▀██░
        ▒▓▓▄ ▄██▒   ▒██▀▀█▄     ░██▄▄▄▄██      ▒   ██▒   ░▓█ ░██
        ▒ ▓███▀ ░   ░██▓ ▒██▒    ▓█   ▓██▒   ▒██████▒▒   ░▓█▒░██▓
        ░ ░▒ ▒  ░   ░ ▒▓ ░▒▓░    ▒▒   ▓▒█░   ▒ ▒▓▒ ▒ ░    ▒ ░░▒░▒
          ░  ▒        ░▒ ░ ▒░     ▒   ▒▒ ░   ░ ░▒  ░ ░    ▒ ░▒░ ░
        ░             ░░   ░      ░   ▒      ░  ░  ░      ░  ░░ ░
        ░ ░            ░              ░  ░         ░      ░  ░  ░
        ░

      pw_async2/dispatcher_base.cc:151: PW_CHECK() or PW_DCHECK() FAILED!

        FAILED ASSERTION

          !task->wakers_.empty()

        FILE & LINE

          pw_async2/dispatcher_base.cc:151

        FUNCTION

          NativeDispatcherBase::RunOneTaskResult pw::async2::NativeDispatcherBase::RunOneTask(Dispatcher &, Task *)

        MESSAGE

          Task 0x7ffd8ddc2f40 returned Pending() without registering a waker

   To prevent obviously incorrect usage, the ``pw_async2`` module asserts if
   you return ``Pending()`` without actually storing a ``Waker``, because it
   means your task has no way of being woken back up.

-------------
Store a waker
-------------
In general, you should always store a  :cc:`Waker <pw::async2::Waker>` before
returning ``Pending()``. A waker is a lightweight object that allows you to
tell the dispatcher to wake a task. When a ``Task::DoPend()`` call returns
``Pending()``, the task is put to sleep so that the dispatcher doesn't have to
repeatedly poll the task. Without a waker, the task will sleep forever.

#. Declare a waker in ``vending_machine.h``:

   * Include the ``pw_async2/waker.h`` header

   * Add a ``pw::async2::Waker waker_`` private member to the ``Keypad`` class

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint3/vending_machine.h
         :language: cpp
         :start-after: #pragma once
         :end-before: // The main task that drives the vending machine.
         :linenos:
         :emphasize-lines: 5,30

#. Use the waker in ``vending_machine.cc``:

   * In ``Keypad::Pend()`` store the waker before returning ``Pending()``:

     .. code-block:: cpp

        PW_ASYNC_STORE_WAKER(cx, waker_, "keypad press");

   The last argument should always be a meaningful string describing the
   wait reason. In the next section you'll see how this string can help you
   debug issues.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint3/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<int> Keypad::Pend(pw::async2::Context& cx) {
         :end-before: pw::async2::Poll<> VendingMachineTask::DoPend
         :linenos:
         :emphasize-lines: 8

.. tip::

   See :ref:`module-pw_async2-guides-primitives-wakers` for an overview of all
   of the different ways that you can set up wakers.

-----------------------
Forget to wake the task
-----------------------
You've set up the waker but you're not using it yet. Let's see what happens.

#. Run the app:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Press :kbd:`c` :kbd:`Enter` to insert a coin.

#. Press :kbd:`1` :kbd:`Enter` to select an item.

   You should see output like this:

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      c
      INF  Received 1 coin.
      INF  Please press a keypad key.
      1

   Nothing happens, not even an assertion! This time there is no crash!
   Why??

   .. dropdown:: Answer

      The problem is that ``pw_async2`` has **no way of knowing when the task
      is ready to be woken up**.

      The reason ``pw_async2`` can detect forgetting to store the waker, on
      the other hand, is because it happens during a ``pw_async2``-initiated
      call into your code, so there can be a postcondition check.

#. Debug this issue by pressing :kbd:`d` :kbd:`Enter`:

   You should see output like this:

   .. code-block:: none

      d
      INF  pw::async2::Dispatcher
      INF  Woken tasks:
      INF  Sleeping tasks:
      INF    - VendingMachineTask:0x7ffeec48fd90 (1 wakers)
      INF      * Waker 1: keypad press

   This shows the state of all the tasks registered with the dispatcher.
   The last line is the wait reason string that you provided when you registered
   the waker. We can see that the vending machine task is still sleeping.

   .. tip::

      If you use ``pw_async2`` in your own project, you can get this kind of
      debug information by calling :cc:`LogRegisteredTasks
      <pw::async2::Dispatcher::LogRegisteredTasks>`.

      If you don't see the reason messages, make sure that
      :cc:`PW_ASYNC2_DEBUG_WAIT_REASON` is not set to ``0``.

-------------
Wake the task
-------------
#. Fix the issue in ``vending_machine.cc``:

   * Invoke the :cc:`Wake() <pw::async2::Waker::Wake>` method on the ``Waker``:

     .. literalinclude:: ./checkpoint4/vending_machine.cc
        :language: cpp
        :start-at: void Keypad::Press(int key) {
        :end-at: }
        :linenos:
        :emphasize-lines: 4

     By design, the ``Wake()`` call consumes the ``Waker``. To invoke ``Wake()``
     you must use ``std::move()`` to make it clear that the task can be only
     woken once through a given waker.

     Wakers are default-constructed in an empty state, and moving the value
     means the location that is moved from is reset to an empty state. If you
     invoke ``Wake()`` on an empty ``Waker``, the call is a no-op.

#. Verify the fix:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Press :kbd:`c` :kbd:`Enter` to insert a coin.

#. Press :kbd:`1` :kbd:`Enter` to select an item.

   You should see keypad input working correctly now:

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      c
      INF  Received 1 coin.
      INF  Please press a keypad key.
      1
      INF  Keypad 1 was pressed. Dispensing an item.

.. tip::

   You can also end up in a "task not waking up" state if you destroy or
   otherwise clear the ``Waker`` instance that pointed at the task to wake.
   ``LogRegisteredTasks()`` can also help here by pointing to a problem related
   to waking your task.

----------
Next steps
----------
Continue to :ref:`module-pw_async2-codelab-step4` to learn how to manage the
rapidly increasing complexity of your code.

.. _module-pw_async2-codelab-step3-checkpoint:

----------
Checkpoint
----------
At this point, your code should look similar to the files below.

.. tab-set::

   .. tab-item:: main.cc

      .. literalinclude:: ./checkpoint4/main.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.cc

      .. literalinclude:: ./checkpoint4/vending_machine.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.h

      .. literalinclude:: ./checkpoint4/vending_machine.h
         :language: cpp
         :start-after: // the License.
