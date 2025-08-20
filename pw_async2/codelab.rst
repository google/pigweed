.. _module-pw_async2-codelab:

=======
Codelab
=======
.. pigweed-module-subpage::
   :name: pw_async2

Welcome to the ``pw_async2`` codelab!

This codelab provides a hands-on introduction to Pigweed's cooperative
asynchronous framework. You will build a simple, simulated "Vending Machine"
application --- an automated machine that asynchronously waits for user input
like coin insertion and keypad presses before dispensing an item. Along the
way, you'll learn the core concepts of ``pw_async2``.

By the end of this codelab, you will know how to:

*   Implement a ``pw::async2::Task`` as a state machine.
*   Call asynchronous functions and manage state across suspension points.
*   Write your own pendable functions that use a ``Waker`` to handle external
    events.
*   Use ``pw::async2::OnceSender`` and ``pw::async2::OnceReceiver`` for basic
    inter-task communication.
*   Use ``pw::async2::TimeProvider`` and ``pw::async2::Select`` to implement
    timeouts.

Let's get started!

.. tip::

   We encourage you to implement each step on your own, but if you
   ever get stuck, a solution is provided at the start of each step.

-----
Setup
-----
The code for this codelab is part of the Pigweed repository. If you haven't
already, follow the `contributor guide <https://pigweed.dev/contributing/#clone-the-repo>`_
to clone the Pigweed repository and set up your development environment.

---------------------------
Step 1: Hello, Async World!
---------------------------
The first step is to create and run a basic asynchronous task. This will
introduce you to the two most fundamental components of ``pw_async2``: the
**Task** and the **Dispatcher**.

What's a Task?
==============
A :doxylink:`pw::async2::Task` is the basic unit of execution in this
framework. It's an object that represents a job to be done, like blinking an
LED, processing sensor data, or, in our case, running a vending machine.

Tasks are implemented by inheriting from the ``pw::async2::Task`` class and
implementing a single virtual method: ``DoPend()``. This method is where the
task's logic lives.

Let's look at the code.

1. Open `pw_async2/codelab/vending_machine.h <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.h>`_

You'll see the definition for our ``VendingMachineTask``:

.. literalinclude:: codelab/vending_machine.h
  :language: cpp
  :linenos:
  :lines: 14-

It's a simple class that inherits from ``pw::async2::Task``. The important part
is the ``DoPend`` method, which is where we'll add our logic.

2. Open `pw_async2/codelab/vending_machine.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.cc>`_

Here you'll find the incomplete implementation of ``DoPend``:

.. literalinclude:: codelab/vending_machine.cc
  :language: cpp
  :linenos:
  :lines: 14-

The ``DoPend`` method returns a :doxylink:`pw::async2::Poll<>`. A ``Poll`` can
be in one of two states:

*   ``Ready()``: The task has finished its work.
*   ``Pending()``: The task is not yet finished and should be run again later.

Our stub currently simply returns ``Ready()``, meaning it would exit
immediately without doing any work.

What's a Dispatcher?
====================
A :doxylink:`pw::async2::Dispatcher` is the engine that runs the tasks. It's a
simple, cooperative scheduler. You give it tasks by calling ``Post()``, and
then you tell it to run them by calling ``RunUntilStalled()`` or
``RunToCompletion()``.

The dispatcher maintains a queue of tasks that are ready to be polled. When a
run is triggered, it pulls a task from the queue and invokes its ``DoPend()``
method. If the task returns ``Pending()``, the task is put to sleep until it
is woken by the operation that blocked it. If it returns ``Ready()``, the
dispatcher considers it complete and will not run it again.

3. Open `pw_async2/codelab/main.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/main.cc>`_

Here you can see we have a dispatcher, but it's not doing anything yet.

.. literalinclude:: codelab/main.cc
  :language: cpp
  :linenos:
  :lines: 14-

Putting it all together
=======================
Now, let's modify the code to print a welcome message.

In `pw_async2/codelab/vending_machine.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.cc>`_, change ``DoPend`` to log the
message:

.. code-block::

   Welcome to the Pigweed Vending Machine!

Keep the ``Ready()`` return, telling the dispatcher to complete the task after
it has logged.

5. Post and Run the Task

In `pw_async2/codelab/main.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/main.cc>`_, create an instance of your vending machine
task and give it to the dispatcher to run.

.. literalinclude:: codelab/solutions/step1/main.cc
  :language: cpp
  :linenos:
  :lines: 14-

Here, ``dispatcher.Post(task)`` adds our task to the dispatcher's run queue.
``dispatcher.RunToCompletion()`` tells the dispatcher to run all of its tasks
until they have all returned ``Ready()``.

**6. Build and Run**

Now, build and run the codelab target from the root of the Pigweed repository:

.. code-block:: sh

   bazelisk run //pw_async2/codelab

You should see the following output:

.. code-block::

   INF  Welcome to the Pigweed Vending Machine!

Congratulations! You've written and run your first asynchronous task with
``pw_async2``. In the next step, you'll learn how to have your task call run
asynchronous operations.

---------------------------------
Step 2: Calling an async function
---------------------------------
In the last step, our task ran from start to finish without stopping. Most
real-world tasks, however, need to wait for things: a timer to expire, a network
packet to arrive, or, in our case, a user to insert a coin.

In ``pw_async2``, operations that can wait are called **pendable functions**.

.. _//pw_async2/codelab/solutions/step2/: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/solutions/step2/vending_machine.cc

.. tip::

   Solution for this step:
   `//pw_async2/codelab/solutions/step2/`_

What's a Pendable function?
===========================
A pendable function is a function that, like a ``Task`` implementation's
``DoPend`` method, takes an async :doxylink:`Context <pw::async2::Context>` and
returns a ``Poll`` of some value. When a task calls a pendable function, it
checks the return value to determine how to proceed.

*   If it's ``Ready(value)``, the operation is complete, and the task can
    continue with the ``value``.

*   If it's ``Pending()``, the operation is not yet complete. The task will
    generally stop and return ``Pending()`` itself, effectively "sleeping" until
    it is woken up.

When a task is sleeping, it doesn't consume any CPU cycles. The ``Dispatcher``
simply won't poll it again until an external event wakes it up. This is the core
of cooperative multitasking.

For this step, we've provided a ``CoinSlot`` class with a pendable function to
read the number of coins inserted:
``pw::async2::Poll<unsigned> Pend(pw::async2::Context& cx)``. Let's use it.

1. Add a ``CoinSlot`` to the vending machine
============================================
First, open `pw_async2/codelab/vending_machine.h <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.h>`_.
You'll need to include ``coin_slot.h``. Add a reference to a ``CoinSlot`` as a
member variable of your ``VendingMachineTask`` and update its constructor to
initialize it.

In your `pw_async2/codelab/main.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/main.cc>`_,
we have provided a global ``CoinSlot`` instance. Pass it into your updated task.

2. Wait for a coin
==================
Now, let's modify the task's ``DoPend`` in `pw_async2/codelab/vending_machine.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.cc>`_.
Following your welcome message from Step 1, prompt the user to insert a coin.

To get a coin from the ``CoinSlot``, you'll call its ``Pend`` method using the
:doxylink:`PW_TRY_READY_ASSIGN` macro.

.. topic:: A Closer Look at ``PW_TRY_READY_ASSIGN``
   :class: tip

   This macro simplifies writing clean asynchronous code in ``pw_async2``.
   It polls a pendable function and handles the two possible outcomes:

   1. If the function returns ``Pending()``, the macro immediately
      returns ``Pending()`` from the current function (your ``DoPend``).
      This propagates the "sleeping" state up to the dispatcher.
   2. If the function returns ``Ready(some_value)``, the macro unwraps
      the value and assigns it to a variable you specify. The task then
      continues executing.

   Without the macro, you would have to write this boilerplate yourself:

   .. code-block:: cpp

      Poll<unsigned> poll_result = coin_slot_.Pend(cx);
      if (poll_result.IsPending()) {
        return Pending();
      }
      unsigned coins = poll_result.value();

   The macro condenses this into a single, expressive line:

   .. code-block:: cpp

      PW_TRY_READY_ASSIGN(unsigned coins, coin_slot_.Pend(cx));

   For those familiar with ``async/await`` in other languages like Rust or
   Python, this macro serves a similar purpose to the ``await`` keyword.
   It's the point at which your task can be suspended.

Use the macro to poll ``coin_slot_.Pend(cx)``. If it's ready, log that a coin
was detected and that an item is being dispensed. Finally, return
``pw::async2::Ready()`` to finish the task.

3. Build and run: Spot the issue
================================
Run your vending machine as before:

.. code-block:: sh

   bazelisk run //pw_async2/codelab

You will see the welcome message, and then the application will wait for your
input.

.. code-block::

   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.

To simulate inserting a coin, type :kbd:`c` and press :kbd:`Enter` in the same
terminal. The hardware thread will call the coin slot ISR, which wakes up your
task. The dispatcher will run it again, and you'll see... an unexpected result:

.. code-block::

   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.
   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.
   INF  Received 1 coin. Dispensing an item.

The welcome message was printed twice! Why?

When a task is suspended and resumed, its ``DoPend`` method is called again
**from the beginning**. The first time ``DoPend`` ran, it printed the welcome
message and then returned ``Pending()`` from inside the ``PW_TRY_READY_ASSIGN``
macro. When the coin was inserted, the task was woken up and the dispatcher
called ``DoPend`` again from the top. It printed the welcome message a second
time, and then when it called ``coin_slot_.Pend(cx)``, the coin was available,
so it returned ``Ready()`` and the task completed.

This demonstrates a critical concept of asynchronous programming: tasks must
manage their own state.

4. Managing the welcome state
=============================
Because a task can be suspended and resumed at any ``Pending()`` return, you
need a way to remember where you left off. For simple cases like this, a boolean
flag is sufficient.

Open `pw_async2/codelab/vending_machine.h <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.h>`_
and add a boolean to track whether the welcome message has been displayed.
Initialize it to ``false``.

Now, modify ``DoPend`` in `pw_async2/codelab/vending_machine.cc <https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.cc>`_.
Gate the two log calls for the welcome message behind your new boolean flag.
Once the message is printed, make sure to set the flag to ``true`` so it won't
be printed again.

7. Build and run: Verify the fix
================================
.. code-block:: sh

   bazelisk run //pw_async2/codelab

Now, the output should be correct. The welcome message is printed once, the
task waits, and then it finishes after you insert a coin.

.. code-block::

   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.

Type :kbd:`c` and press :kbd:`Enter`:

.. code-block::

   INF  Received 1 coin. Dispensing an item.

The task then completes, ``RunToCompletion`` returns, and the program exits.

You've now implemented a task that can wait for an asynchronous event and
correctly manages its state! In the next step, you'll learn how to write your
own pendable functions.
