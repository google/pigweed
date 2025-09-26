.. _module-pw_async2-codelab-step4:

======================
4. Use a state machine
======================
.. pigweed-module-subpage::
   :name: pw_async2

The ``DoPend()`` implementation in your ``VendingMachineTask`` now:

* Displays a welcome message
* Prompts the user to insert a coin… unless they have been prompted already
* Waits for the user to insert a coin… unless coins have been inserted already
* Waits for the user to select an item with the keypad

Implementing ``DoPend()`` like this is valid, but you can imagine the chain of
checks growing ever longer as the complexity increases. You'd end up with a
long list of specialized conditional checks to skip the early stages before you
handle the later stages. It's also not ideal that you can't process keypad input
while waiting for coins to be inserted.

To manage this complexity, we recommend explicitly structuring your tasks as
state machines.

-------------------------------------
Refactor your task as a state machine
-------------------------------------
#. Set up the state machine data in ``vending_machine.h``:

   * Declare a private ``State`` enum in ``VendingMachineTask`` that represents
     all possible states:

     * ``kWelcome``

     * ``kAwaitingPayment``

     * ``kAwaitingSelection``

   * Declare a private ``State state_`` data member that stores the current
     state

   * Initialize ``state_`` in the ``VendingMachineTask`` constructor

   * Remove the ``displayed_welcome_message_`` data member which is no longer
     needed (a good sign!) and remove its initialization in the constructor

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-at: class VendingMachineTask
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 6,9,17,21-26

#. Refactor the ``DoPend()`` implementation in ``vending_machine.cc`` as an
   explicit state machine:

   * Handle the ``kWelcome``, ``kAwaitingPayment``, and ``kAwaitingSelection``
     states (as well as the transitions between states)

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-at: pw::async2::Poll<> VendingMachineTask::DoPend
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 2-24

   This isn't the only way to do it, but it is perhaps the easiest to understand
   since there isn't a lot of hidden machinery.

.. topic:: State machines in C++

   Two other options for implementing a state machine in C++ include:

   * Define a type tag for each state, use ``std::variant`` to represent the
     possible states, and use ``std::visit`` to dispatch to a handler for each
     state. The compiler effectively generates the switch statement for you at
     compile-time.

   * Use runtime dispatch through a function pointer to handle each state.
     Usually you create a base class that defines a virtual function, and you
     derive each state by overriding the virtual function.

------------------------
Problem with linear flow
------------------------
Even with the task refactored as an explicit state machine, there's still a
problem with the simple linear flow we've implemented so far. Watch what
happens when you make a selection before inserting coins:

#. Press :kbd:`1` :kbd:`c` :kbd:`Enter`.

   You should see output like this:

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!
      INF  Please insert a coin.
      1c
      INF  Received 1 coin.
      INF  Please press a keypad key.
      INF  Keypad 1 was pressed. Dispensing an item.

When the vending machine received a coin, it immediately dispensed an item
based on keypad input that occurred before the coin insertion. Imagine inserting
money into a real vending machine, and then it automatically dispenses some
random item, because someone else had previously come along an hour ago and
touched the button for that item!

In other words, how do you make your task better at handling multiple inputs
when ``CoinSlot`` and ``Keypad`` can each only wait on one thing?

The answer is to use the :cc:`Selector <pw::async2::Selector>` class and
the :cc:`Select <pw::async2::Select>` helper function to wait on multiple
pendables, along with the :cc:`VisitSelectResult
<pw::async2::VisitSelectResult>` helper that allows you to unpack the
completion result value when one of those pendables returns ``Ready()``.

--------------------------
Wait on a set of pendables
--------------------------
When multiple states use the same set of pendables (e.g. ``kAwaitingPayment``
and ``kAwaitingSelection`` both use ``CoinSlot`` and ``Keypad``) it's best to
encapsulate the calls into a function that both states can use. The code for
waiting on a set of pendables is template-heavy, which can lead to lots of
compiler-generated code. Encapsulating the calls into a function reduces the
number of times that templates need to be expanded.

For the vending machine, we can unify coin insertions and keypad selections
using an ``Input`` enum, and a function ``PendInput`` for pending on either.

#. Set up the ``Input`` enum in ``vending_machine.h``:

   * Add an ``Input`` enum with the following states:

     * ``kNone``

     * ``kCoinInserted``

     * ``kKeyPressed``

   * Declare a new ``PendInput()`` method:

     .. literalinclude:: ./checkpoint2/vending_machine.h
        :language: cpp
        :start-at: Waits for either an inserted coin or keypress
        :end-at: pw::async2::Poll<Input> PendInput(pw::async2::Context& cx);
        :linenos:
        :emphasize-lines: 3

   * Include the ``<optional>`` header

   * Add a new ``std::optional<int> selected_item_`` data member

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint2/vending_machine.h
         :language: cpp
         :start-at: class VendingMachineTask
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 11-15,23,36

#. Update the ``VendingMachineTask`` implementation in ``vending_machine.cc``:

   * Implement ``PendInput()``:

     .. literalinclude:: ./checkpoint2/vending_machine.cc
        :language: cpp
        :start-at: pw::async2::Poll<VendingMachineTask::Input> VendingMachineTask::PendInput
        :end-before: pw::async2::Poll<> VendingMachineTask::DoPend
        :linenos:
        :emphasize-lines: 3

   * Refactor ``DoPend()`` to use ``Input``:

     .. literalinclude:: ./checkpoint2/vending_machine.cc
        :language: cpp
        :start-at: pw::async2::Poll<> VendingMachineTask::DoPend
        :end-before: }  // namespace codelab
        :linenos:

Get yourself a ``$FAVORITE_BEVERAGE``. There's a lot to explain.

Let's start with the ``PendInput()`` implementation. The
:cc:`PW_TRY_READY_ASSIGN` invocation uses :cc:`Select() <pw::async2::Select>`
to wait on the coin insertion and keypad press pendables:

.. literalinclude:: ./checkpoint2/vending_machine.cc
  :language: cpp
  :start-after: selected_item_ = std::nullopt;
  :end-before: pw::async2::VisitSelectResult(
  :linenos:

As usual, we're using ``PW_TRY_READY_ASSIGN`` so that if all the pendables are
pending then the current function will return ``Pending()``.

We use ``auto`` for the result return type because the actual type is very
complicated. Typing out the entire type would be laborious and would reduce
code readability.

``Select()`` checks the pendables in the order provided. ``result`` will only
contain a single result, from whatever pendable was ready. To get another
result you'd have to call ``Select()`` again.

Note that each pendable doesn't have a fair chance to do work. The first
pendable gets polled first, and if it's ready, it takes precedence.

.. note::

   :cc:`Selector <pw::async2::Selector>` is another way to wait on a set of
   pendables. It's a pendable class that polls an ordered set of pendables you
   provide to determine which (if any) are ready.

   If you construct and store a ``Selector`` instance yourself, you can give all
   the pendables in the poll set a chance to return ``Ready()``, since each will
   be polled until the first time it returns ``Ready()``. However you must
   arrange to invoke the ``Pend()`` function on the same ``Selector`` instance
   yourself in a loop.

   Once you process the ``AllPendablesCompleted`` result when using
   ``VisitSelectResult()`` (explained below), you could then reset the
   ``Selector`` to once again try all the pendables again.

The :cc:`PendableFor <pw::async2::PendableFor>` helper function used in the
``Select()`` invocation constructs a type-erased wrapper that makes it easy to
call the ``Pend()`` method from any pendable.

Moving on, :cc:`VisitSelectResult <pw::async2::VisitSelectResult>` is a helper
for processing the result of the ``Select`` function or the ``Selector::Pend()``
member function call:

.. literalinclude:: ./checkpoint2/vending_machine.cc
  :language: cpp
  :start-at: pw::async2::VisitSelectResult(
  :end-before: return input;
  :linenos:

``result`` contains a single ``Ready()`` result, but because of how the result
is stored, there is a bit of C++ template magic to unpack it for each possible
type. ``VisitSelectResult()`` does its best to hide most of the details, but you
need to specify an ordered list of lambda functions to handle each specific
pendable result. The order must match the ordering that was used in the
``Select()`` call:

.. code-block:: cpp

   pw::async2::VisitSelectResult(
       result,
       [](pw::async2::AllPendablesCompleted) {
         // Special lambda that's only needed when using `Selector::Pend()`, and
         // which is invoked when all the other pendables have completed.
         // This can be left blank when using `Select()` as it is not used.
       },
       [&](unsigned coins) {
         // This is the first lambda after the `AllPendablesCompleted`` case as
         // `CoinSlot::Pend()` was in the first argument to `Select`.
         // Invoked if the `CoinSlot::Pend()` is ready, with the count of coins
         // returned as part of the `Poll` result from that call.
       },
       [&](int key) {
         // This is the second lambda after the `AllPendablesCompleted`` case as
         // `Keypad::Pend()` was in the second argument to `Select()`.
         // Invoked if the `Keypad::Pend()` is ready, with the key number
         // returned as part of the `Poll` result from that call.
       });

.. note::

   In case you were curious about the syntax, behind the scenes a ``std::visit``
   is used with a ``std::variant``, and lambdas like these are how you can deal
   with the alternative values.

As for the updates to ``DoPend()``, it's mostly the same logic as before, with
the addition of some more ``switch`` logic to handle the new ``Input``
encapsulation.

----------
Next steps
----------
Continue to :ref:`module-pw_async2-codelab-step5` to learn how to spin up a
new task and get your tasks communicating with each other.

.. _module-pw_async2-codelab-step4-checkpoint:

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
