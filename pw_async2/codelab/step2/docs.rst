.. _module-pw_async2-codelab-step2:

=========================
2. Call an async function
=========================
.. pigweed-module-subpage::
   :name: pw_async2

The task that you created in the last step isn't actually asynchronous
yet. It runs from start to finish without stopping. Most real-world tasks
need to wait for one reason or another. For example, one task may need to wait
for a timer to expire. Another one might need to wait for a network packet to
arrive. In the case of the vending machine app, your task needs to wait for a
user to insert a coin.

In ``pw_async2``, operations that can wait are called **pendable functions**.

---------------
Wait for a coin
---------------
Your vending machine will use the ``CoinSlot`` class to read the number of coins
that a customer inserts. We've already implemented this class for you. Update
your ``VendingMachineTask`` to use ``CoinSlot`` now.

#. Study the ``Pend()`` declaration in :cs:`pw_async2/codelab/coin_slot.h`.

   .. literalinclude:: ../coin_slot.h
      :language: cpp
      :start-after: constexpr CoinSlot() : coins_deposited_(0) {}
      :end-before: // Report that a coin was received by the coin slot.
      :linenos:

   Similar to ``DoPend()``, the coin slot's ``Pend()`` method is a pendable
   function that takes an async :cc:`Context <pw::async2::Context>` and returns
   a ``Poll`` of some value. When a task calls a pendable function, it checks
   the return value to determine how to proceed:

   * If it's ``Ready(value)``, the operation is complete, and the task can
     continue with the ``value``.

   * If it's ``Pending()``, the operation is not yet complete. The task will
     generally stop and return ``Pending()`` itself, effectively "sleeping" until
     it is woken up.

   When a task is sleeping, it doesn't consume any CPU cycles. The
   ``Dispatcher`` won't poll it again until an external event wakes it
   up. This is the core of cooperative multitasking.

   .. note::

      If you peek into :cs:`pw_async2/codelab/coin_slot.cc` you'll see that the
      ``Pend()`` implementation uses something called a waker. You'll learn more
      about wakers in the next step.

#. Update the ``VendingMachineTask`` declaration in ``vending_machine.h``
   to use ``CoinSlot``:

   * Include the ``coin_slot.h`` header

   * Update the ``VendingMachineTask`` constructor to accept a coin slot
     reference (``CoinSlot& coin_slot``)

   * Add a ``coin_slot_`` data member to ``VendingMachineTask``

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-after: #pragma once
         :linenos:
         :emphasize-lines: 1,11,13,20

#. Update ``main.cc``:

   * When creating your ``VendingMachineTask`` instance, pass the coin
     slot as an arg

   We've already provided a global ``CoinSlot`` instance (``coin_slot``) at
   the top of ``main.cc``. You don't need to create one.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/main.cc
         :language: cpp
         :start-at: int main() {
         :emphasize-lines: 5
         :linenos:

#. Update the ``DoPend()`` implementation in ``vending_machine.cc`` to interact
   with the coin slot:

   * Prompt the user to insert a coin:

     .. code-block:: cpp

        PW_LOG_INFO("Please insert a coin.");

   * Use ``coin_slot_.Pend(cx)`` to wait for coin insertion

   * Handle the pending case of ``coin_slot_.Pend(cx)``

   * If ``coin_slot_.Pend(cx)`` is ready, log the number of coins that
     were detected

   Recall that ``CoinSlot::Pend`` returns ``Poll<unsigned>`` indicating the
   status of the coin slot:

   * If ``Poll()`` returns ``Pending()``, it means that no coin has been
     inserted yet. Your task cannot proceed without payment, so it must signal
     this to the dispatcher by returning ``Pending()`` itself. Pendable
     functions like ``CoinSlot::Pend`` which wait for data will automatically
     wake your waiting task once that data becomes available.

   * If the ``Poll`` is ``Ready()``, it means that coins have been inserted. The
     ``Poll`` object now contains the number of coins. Your task can get this
     value and proceed to the next step.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<> VendingMachineTask::DoPend
         :emphasize-lines: 3-11
         :linenos:

------------------
Reduce boilerplate
------------------
The pattern of polling a pendable function and returning ``Pending()`` if
it's not ready is common in ``pw_async2``. To reduce this boilerplate,
``pw_async2`` provides the :cc:`PW_TRY_READY_ASSIGN` macro to simplify writing
clean async code.

#. Refactor the ``DoPend()`` implementation in ``vending_machine.cc``:

   * Replace the code that you wrote in the last step with a
     :cc:`PW_TRY_READY_ASSIGN` implementation that handles both the ready and
     pending scenarios.

     1. If the function returns ``Pending()``, the macro immediately
        returns ``Pending()`` from the current function (your ``DoPend``).
        This propagates the "sleeping" state up to the dispatcher.

     2. If the function returns ``Ready(some_value)``, the macro unwraps
        the value and assigns it to a variable you specify. The task then
        continues executing.

   For those familiar with ``async/await`` in other languages like Rust or
   Python, this macro serves a similar purpose to the ``await`` keyword.
   It's the point at which your task can be suspended.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint2/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<> VendingMachineTask::DoPend
         :emphasize-lines: 4
         :linenos:

--------------
Spot the issue
--------------
There's a problem with the current implementation…

#. Run your vending machine app again:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

   You should see the welcome message, followed by the prompt to insert coins.

   .. code-block::

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.

#. To simulate inserting a coin, type :kbd:`c` :kbd:`Enter` (type :kbd:`c`
   and then type :kbd:`Enter`):

   The hardware thread will call the coin slot's Interrupt Service Routine
   (ISR), which wakes up your task. The dispatcher will run the task again, and
   you'll see… an unexpected result:

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      c
      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      INF  Received 1 coin. Dispensing item.

   The welcome message was printed twice! Why?

   .. dropdown:: Answer

      When a task is suspended and resumed, its ``DoPend()`` method is called
      again *from the beginning*. The first time ``DoPend()`` ran, it printed
      the welcome message and then returned ``Pending()`` from inside the
      ``PW_TRY_READY_ASSIGN`` macro. When the coin was inserted, the task was
      woken up and the dispatcher called ``DoPend()`` again from the top. It
      printed the welcome message a second time, and then when it called
      ``coin_slot_.Pend(cx)``, the coin was available, so it returned
      ``Ready()`` and the task completed.

      This demonstrates a critical concept of asynchronous programming: **tasks
      must manage their own state**.

------------------------
Manage the welcome state
------------------------
Because a task can be suspended and resumed at any ``Pending()`` return, you
need a way to remember where you left off. For simple cases like this, a boolean
flag is sufficient.

#. Add the boolean flag in ``vending_machine.h``:

   * Add a ``displayed_welcome_message_`` data member to ``VendingMachineTask``

   * Initialize ``displayed_welcome_message_`` to ``false`` in the constructor

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint3/vending_machine.h
         :language: cpp
         :start-at: class VendingMachineTask
         :end-before: }  // namespace codelab
         :emphasize-lines: 6,14
         :linenos:

#. Update ``vending_machine.cc``:

   * Gate the welcome message and coin insertion prompt in ``DoPend()`` behind
     the boolean flag

   * Flip the flag after the welcome message and prompt have been printed

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint3/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<> VendingMachineTask::DoPend
         :end-before: }  // namespace codelab
         :emphasize-lines: 2-6
         :linenos:

--------------
Verify the fix
--------------
The welcome message should no longer get duplicated.

#. Run the app again:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

#. Simulate inserting a coin by pressing :kbd:`c` :kbd:`Enter`.

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      c
      INF  Received 1 coin. Dispensing item.

----------
Next steps
----------
You've now implemented a task that waits for an asynchronous event and
correctly manages its state. In :ref:`module-pw_async2-codelab-step3`, you'll
learn how to write your own pendable functions.

.. _module-pw_async2-codelab-step2-checkpoint:

----------
Checkpoint
----------
At this point, your code should look similar to the files below.

.. tab-set::

   .. tab-item:: main.cc

      .. literalinclude:: ./checkpoint3/main.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.cc

      .. literalinclude:: ./checkpoint3/vending_machine.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.h

      .. literalinclude:: ./checkpoint3/vending_machine.h
         :language: cpp
         :start-after: // the License.
