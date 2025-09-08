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

-----
Setup
-----
The code for this codelab is part of the Pigweed repository. If you haven't
already, follow the :ref:`contributor guide <docs-contributing-setup>` to clone
the Pigweed repository and set up your development environment.

.. tip::

   We encourage you to implement each step on your own, but if you
   ever get stuck, a solution is provided at the start of each step.

---------------------------
Step 1: Hello, Async World!
---------------------------
The first step is to create and run a basic asynchronous task. This will
introduce you to the two most fundamental components of ``pw_async2``: the
**Task** and the **Dispatcher**.

.. admonition:: Solution for this step
   :class: hint

   `//pw_async2/codelab/solutions/step1`_

What's a Task?
==============
A :doxylink:`pw::async2::Task` is the basic unit of execution in this
framework. It's an object that represents a job to be done, like blinking an
LED, processing sensor data, or, in our case, running a vending machine.

Tasks are implemented by inheriting from the ``pw::async2::Task`` class and
implementing a single virtual method: ``DoPend()``. This method is where the
task's logic lives.

Let's look at the code.

1. Open `//pw_async2/codelab/vending_machine.h`_

You'll see the definition for our ``VendingMachineTask``:

.. literalinclude:: codelab/vending_machine.h
  :language: cpp
  :linenos:
  :lines: 14-

It's a simple class that inherits from ``pw::async2::Task``. The important part
is the ``DoPend`` method, which is where we'll add our logic.

2. Open `//pw_async2/codelab/vending_machine.cc`_

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

3. Open `//pw_async2/codelab/main.cc`_

Here you can see we have a dispatcher, but it's not doing anything yet.

.. literalinclude:: codelab/main.cc
  :language: cpp
  :linenos:
  :lines: 14-

Putting it all together
=======================
Now, let's modify the code to print a welcome message.

In `//pw_async2/codelab/vending_machine.cc`_, change ``DoPend`` to log the
message:

.. code-block::

   Welcome to the Pigweed Vending Machine!

Keep the ``Ready()`` return, telling the dispatcher to complete the task after
it has logged.

5. Post and Run the Task

In `//pw_async2/codelab/main.cc`_, create an instance of your vending machine
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
``pw_async2``. In the next step, you'll learn how to have your task run
asynchronous operations.

---------------------------------
Step 2: Calling an async function
---------------------------------
In the last step, our task ran from start to finish without stopping. Most
real-world tasks, however, need to wait for things: a timer to expire, a network
packet to arrive, or, in our case, a user to insert a coin.

In ``pw_async2``, operations that can wait are called **pendable functions**.

.. admonition:: Solution for this step
   :class: hint

   `//pw_async2/codelab/solutions/step2`_

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
First, open `//pw_async2/codelab/vending_machine.h`_. You'll need to include
``coin_slot.h``. Add a reference to a ``CoinSlot`` as a member variable of your
``VendingMachineTask`` and update its constructor to initialize it.

In your `//pw_async2/codelab/main.cc`_, we have provided a global ``CoinSlot``
instance. Pass it into your updated task.

2. Wait for a coin
==================
Now, let's modify the task's ``DoPend`` in
`//pw_async2/codelab/vending_machine.cc`_. Following your welcome message from
Step 1, prompt the user to insert a coin.

To wait for a coin from the ``CoinSlot``, you'll call its ``Pend`` function.
This returns a ``Poll<unsigned>`` indicating the status of the coin slot.

*   If the ``Poll`` is ``Pending()``, it means that no coin has been inserted
    yet. Your task cannot proceed without payment, so it must signal this to
    the dispatcher by returning ``Pending()`` itself. Pendable functions like
    ``CoinSlot::Pend`` which wait for data will automatically wake your waiting
    task once that data becomes available.

*   If the ``Poll`` is ``Ready()``, it means that coins have been inserted. The
    ``Poll`` object now contains the number of coins. Your task can get this
    value and proceed to the next step.

Here's how you would write that:

.. code-block:: cpp

   pw::async2::Poll<unsigned> poll_result = coin_slot_.Pend(cx);
   if (poll_result.IsPending()) {
     return pw::async2::Pending();
   }
   unsigned coins = poll_result.value();

Add this code to your ``DoPend`` method. After getting the number of coins, log
that a coin was detected and that an item is being dispensed. Finally, return
``pw::async2::Ready()`` to finish the task.

.. topic:: Simplifying with ``PW_TRY_READY_ASSIGN``
   :class: tip

   The pattern of polling a pendable function and returning ``Pending()`` if
   it's not ready is common in ``pw_async2``. To reduce this boilerplate,
   ``pw_async2`` provides the :doxylink:`PW_TRY_READY_ASSIGN` macro.

   This macro simplifies writing clean asynchronous code. It polls a pendable
   function and handles the two possible outcomes:

   1. If the function returns ``Pending()``, the macro immediately
      returns ``Pending()`` from the current function (your ``DoPend``).
      This propagates the "sleeping" state up to the dispatcher.
   2. If the function returns ``Ready(some_value)``, the macro unwraps
      the value and assigns it to a variable you specify. The task then
      continues executing.

   The four lines of code you just wrote can be condensed into a single,
   expressive line:

   .. code-block:: cpp

      PW_TRY_READY_ASSIGN(unsigned coins, coin_slot_.Pend(cx));

   For those familiar with ``async/await`` in other languages like Rust or
   Python, this macro serves a similar purpose to the ``await`` keyword.
   It's the point at which your task can be suspended.

Go ahead and replace the call to the ``CoinSlot`` in your ``DoPend`` with this
macro. The behavior will be identical, but the code is much cleaner.

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
terminal. The hardware thread will call the coin slot Interrupt Service Routine
(ISR), which wakes up your task. The dispatcher will run it again, and you'll
see… an unexpected result:

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

Open `//pw_async2/codelab/vending_machine.h`_ and add a boolean to track whether
the welcome message has been displayed. Initialize it to ``false``.

Now, modify ``DoPend`` in `//pw_async2/codelab/vending_machine.cc`_. Gate the
two log calls for the welcome message behind your new boolean flag. Once the
message is printed, make sure to set the flag to ``true`` so it won't be printed
again.

5. Build and run: Verify the fix
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

--------------------------------------
Step 3: Writing your own event handler
--------------------------------------
In the last step, you created a task to dispense an item after a coin was
inserted. Most vending machines at least allow you to choose what to buy.
Let's fix that by handling the keypad input ISR, and using the key press info
in the task to dispense an item.

Along the way you will learn how to correctly wake up a task that is waiting for
input like this. You will also gain some experience implementing a ``Pend()``
function yourself.

The provided hardware simulation will send you a keypad event via an
asynchronous call to the ``key_press_isr()`` that should already be defined in
your `//pw_async2/codelab/main.cc`_ file. It will pass you an integer value in
the range (0-9) to indicate which keypad button was pressed. It is going to be
up to you to process that keypad event safely, and allow your task to wait for
the keypad number after receiving a coin to dispense an item.

A single digit should be enough, but if you want an extra challenge, you can
choose to allow larger numbers to be entered.

.. admonition:: Solution for this step
   :class: hint

   `//pw_async2/codelab/solutions/step3`_

1. Define a stub ``Keypad`` class
=================================
Lets start with a minimal stub implementation. Add the following declaration to
your `//pw_async2/codelab/vending_machine.h`_ header file:

.. code-block:: cpp

   class Keypad {
    public:
     constexpr Keypad() : key_pressed_(kNone) {}

     // Pends until a key has been pressed, returning the key number.
     //
     // May only be called by one task.
     pw::async2::Poll<int> Pend(pw::async2::Context& cx);

     // Record a key press. Typically called from the keypad ISR.
     void Press(int key);

    private:
     // A special internal value to indicate no keypad button has yet been
     // pressed.
     static constexpr int kNone = -1;

     int key_pressed_;
   };

Also add these stub implementations to the top of your
`//pw_async2/codelab/vending_machine.cc`_ file:

.. code-block:: cpp

   pw::async2::Poll<int> Keypad::Pend(pw::async2::Context& cx) {
     // This is a stub implementation!
     static_cast<void>(cx);
     return key_pressed_;
   }

   void Keypad::Press(int key) {
     // This is a stub implementation!
     static_cast<void>(key);
   }

This should be a good starting stub. Notice how the ``Pend`` member function just
immediately returns the value of ``key_pressed_``, which is only ever set to
``kNone``. We will fix that later, but let's integrate the keypad into the rest
of the code first.

2. Add the ``Keypad`` to the vending machine
============================================
In your `//pw_async2/codelab/main.cc`_ file, create a global instance of your
keypad type next to the coin slot instance, and then update your
``VendingMachineTask`` constructor to take a reference to it in the constructor,
and to save the reference as member data.

3. Wait for a key event in your task
====================================
At this point, your task's ``DoPend`` function should look something like the
solution file for step 2 (though not necessarily identical):

.. literalinclude:: codelab/solutions/step2/vending_machine.cc
  :language: cpp
  :linenos:
  :start-after: VendingMachineTask::DoPend
  :end-at: pw::async2::Ready

The logical place to handle the keypad input is after receiving a coin.

Update the coin received message to remove the "item is being dispensed"
message. Instead we will wait for the keypad event.

Waiting for a keypad event is going to be very much like waiting for a coin.
Use the :doxylink:`PW_TRY_READY_ASSIGN` macro to poll ``keypad_.Pend(cx)``. If
it is ready, log the keypad key that was received, and that an item is
dispensing before returning ``pw::async2::Ready()`` to finish the task.

4. Build and verify the stub
============================
Run your vending machine as before:

.. code-block:: sh

   bazelisk run //pw_async2/codelab

You will see the welcome message, and you can insert a coin by again typing
:kbd:`c` and pressing :kbd:`Enter`. You should see a message that "-1" was
pressed. This is expected since the ``KeyPad::DoPend()`` stub implementation
returns ``key_pressed_``, which was initialized to ``kNone`` (-1).

.. code-block:: sh

   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.
   c
   INF  Received 1 coin.
   INF  Please press a keypad key.
   INF  Keypad -1 was pressed. Dispensing an item.

So far so good! Next it is time to handle the hardware event, and have your task
wait for the key press data.

5. Handle the event in your ``Keypad`` implementation
=====================================================
The first step should be trivial. Modify the stub ``key_press_isr`` in your
`//pw_async2/codelab/main.cc`_ to pass the key number to the ``Keypad::Press``
member function.

.. code-block:: cpp

   void key_press_isr(int raw_key_code) { keypad.Press(raw_key_code); }

The next step is harder, implementing the ``Keypad::Press`` member function
correctly.

Since the keypad ISR is asynchronous, you will need to synchronize access to
the stored event data. For this codelab, we use
:doxylink:`pw::sync::InterruptSpinLock` which is safe to acquire from an ISR in
production use. Alternatively you can use atomic operations.

We'll also use ``PW_GUARDED_BY`` to add a compile-time check that the protected
members are accessed with the lock held.

Normally you would have to add the correct dependencies to the
`//pw_async2/codelab/BUILD.bazel`_ file, but we've already included them to save
you some work. But if something went wrong, they are straightforward:

.. code-block:: cpp

   "//pw_sync:interrupt_spin_lock",
   "//pw_sync:lock_annotations",

1. Add an instance of the spin lock to your ``Keypad`` class, along with a
   data member to hold the key press data.

   .. code-block:: cpp

      pw::sync::InterruptSpinLock lock_;
      int key_pressed_ PW_GUARDED_BY(lock_);

   The ``PW_GUARDED_BY(lock_)`` just tells the compiler (clang) that to access
   ``key_pressed_``, ``lock_`` should be held first, otherwise it should emit a
   diagnostic.

2. Add two includes at the top of your `//pw_async2/codelab/vending_machine.h`_:

   .. code-block:: cpp

      #include "pw_sync/interrupt_spin_lock.h"
      #include "pw_sync/lock_annotations.h"

3. Now you can implement ``Keypad::Press`` to save off the event data in a way
   that it can be safely read by ``Keypad::Pend``.

   .. code-block:: cpp

      std::lock_guard lock(lock_);
      key_pressed_ = key;


4. You can start off with this implementation for ``Keypad::Pend``:

   .. code-block:: cpp

      std::lock_guard lock(lock_);
      int key = std::exchange(key_pressed_, kNone);
      if (key != kNone) {
        return key;
      }
      return pw::async2::Pending();

   If you haven't seen ``std::exchange`` used like this before, it just ensures
   that the key pressed event data is read only once by clearing it out to
   ``kNone`` (-1) after reading the value of ``key_pressed_``.

It's so simple… what could go wrong?

.. code-block:: sh

   bazelisk run //pw_async2/codelab

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

6. Fix the crash: Registering a waker
=====================================
The crash message is intentionally blatant about what went wrong. The
implementation of ``Keypad::Pend`` I told you to write was intentionally
incomplete to show you what happens if you make this misstep.

.. topic:: When to store a waker

   Generally if you are writing the leaf logic that decides that
   :doxylink:`pw::async2::Pending` should be returned from your task, then you
   should also be storing a :doxylink:`pw::async2::Waker` at the same time,
   so you can wake up the task when the data you are waiting for is available.

   One ``Waker`` wakes up one task. If your code needs to wake up multiple
   tasks, you should use :doxylink:`pw::async2::WakerQueue` instead of a single
   waker instance.

Let's set up the waiter.

1. First include ``pw_async2/waker.h`` at the top of your
   `//pw_async2/codelab/vending_machine.h`_ header.

2. Add an instance as member data to your ``Keypad`` class.

   Note that the instance is internally thread-safe, and ou do **not** need to
   be guarded by an extra spinlock. An external spinlock is redundant, but
   harmless.

   .. code-block:: cpp

      pw::async2::Waker waker_;

3. Setup the waker right before returning :doxylink:`pw::async2::Pending`

   To do this correctly, you use  :doxylink:`PW_ASYNC_STORE_WAKER`, giving it
   the context argument passed in to the ``Pend()``, the waker to store to, and
   a ``wait_reason_string`` to help debug issues.

   .. tip::

      Pass a meaningful string for last ``wait_reason_string``, as this will
      help you debug issues.

   Here's what the end of ``Keypad::Pend`` should look like:

   .. code-block:: cpp

      PW_ASYNC_STORE_WAKER(cx, waker_, "keypad press");
      return pw::async2::Pending();


We haven't yet modified ``Keypad::Press`` to use the waker yet, and we will need
to. But first let's show what happens if you forget this step. This time there
will not be a crash!

8. Forgetting to wake the task
==============================

Let's see what happens if you forget to wake the task.

Build and run the codelab, and then press :kbd:`c` :kbd:`Enter` :kbd:`1`
:kbd:`Enter`.

.. code-block:: sh

   bazelisk run //pw_async2/codelab

   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.
   c
   INF  Received 1 coin.
   INF  Please press a keypad key.
   1

As expected, nothing happens, not even an assertion. ``pw_async2`` has no way of
knowing itself when the task is ready to be woken up as the pendable is ready.

You might wonder then how you would even debug this problem. Luckily, there is
a way!

Try pressing :kbd:`d` then :kbd:`Enter`.

.. code-block:: sh

   d
   INF  pw::async2::Dispatcher
   INF  Woken tasks:
   INF  Sleeping tasks:
   INF    - VendingMachineTask:0x7ffeec48fd90 (1 wakers)
   INF      * Waker 1: keypad press

This shows the state of all the tasks registered with the dispatcher.

Behind the scenes, the ``hardware.cc`` implementation calls
:doxylink:`LogRegisteredTasks <pw::async2::Dispatcher::LogRegisteredTasks>` on
the dispatcher which was registered via the ``HardwareInit()`` function.

You can make this same call yourself to understand why your tasks aren't doing
anything, and investigate from there.

In this case we know we are sending a keypad press event, but obviously from
the ``Waker 1: keypad press`` line in the output log, the task wasn't properly
woken up.

To fix it, let's add the missing ``Wake()`` call to ``Keypad::Press``:

.. code-block:: cpp

   std::lock_guard lock(lock_);
   key_pressed_ = key;
   std::move(waker_).Wake();

We move the waker out of ``waker_`` first to consume it. That way the next
interrupt won't wake the task if the task is actually waiting on something else.

Note it is completely safe to make the ``Wake()`` call if the waker is in
an "empty" state, such as when no waker has yet been stored using
``PW_ASYNC_STORE_WAKER``. Doing that is a no-op.

.. tip::

   You can also end up in a "task not waking up" state if you destroy or
   otherwise clear the ``Waker`` instance that pointed at the task to wake.
   Again ``LogRegisteredTasks`` will point to a problem waking your task,
   and give the last reason message, so you know where to start looking.

.. important::

   If you don't see the reason messages, you may have configured
   :doxylink:`PW_ASYNC2_DEBUG_WAIT_REASON` to ``0`` to disable them.
   ``LogRegisteredTasks`` will still print out what it can, but for more
   information you may need to consider enabling them temporarily.

9. Verify your event handler
============================
.. code-block:: sh

   bazelisk run //pw_async2/codelab

Does it work as you expect?

.. tip::

   If you suspect you didn't implement your ``Keypad`` class correctly,
   comparing your solution against the  `//pw_async2/codelab/coin_slot.cc`_
   implementation might help before looking at the
   `//pw_async2/codelab/solutions/step3`_ solution.

Well, depending on how you arranged to wait on both the ``CoinSlot`` and
``Keypad`` in your ``DoPend`` implementation, you could have one more problem.

.. topic:: Spot the bug!
   :class: tip

   If the end of your ``DoPend()`` function looks like this, you have a bug.

   .. code-block:: cpp

      PW_TRY_READY_ASSIGN(unsigned coins, coin_slot_.Pend(cx));
      PW_LOG_INFO("Received %u coin%s.", coins, coins > 1 ? "s" : "");
      PW_LOG_INFO("Please press a keypad key.");

      PW_TRY_READY_ASSIGN(int key, keypad_.Pend(cx));
      PW_LOG_INFO("Keypad %d was pressed. Dispensing an item.", key);

      return pw::async2::Ready();

   You will find that after typing :kbd:`c` :kbd:`Enter` to insert a coin, then
   typing :kbd:`1` :kbd:`Enter` to submit a keypad event, you will not see the
   "Dispensing an item" message.

   Try typing :kbd:`d` :kbd:`Enter`, to see what your task is waiting on. Can
   you fix it?

We will look at how to better handle increasing complexity in your ``DoPend``
function in the next step.

-------------------------------
Step 4: Dealing with complexity
-------------------------------

You've now gotten to a point where your ``VendingMachineTask`` has a
``DoPend()`` member function that:

- First displays a welcome message, asking the user to insert a coin.

  - … unless it has been displayed already.

- Then waits for the user to insert a coin.

  - … unless it has been inserted already.

- Then waits for the user to select an item with the keypad.

  - We haven't actually needed it yet, but we might also need to skip this
    if it has already occurred.

Writing ``DoPend()`` functions this way is a perfectly valid choice, but you can
imagine the pattern of a chain of checks growing ever longer as the complexity
increases, and you end up with a long list of specialized conditional checks to
skip the early stages before you handle the later stages.

It's also not ideal that we can't process keypad input while waiting for a coin
to be inserted. It would be nice to do something useful when the user makes a
selection before paying for it. Likewise, when handling keypad input, we may
miss additional coin insertion events when we should handle them, so we can
properly account for the coins we are holding prior to a purchase.

This step shows you how to do this.

.. admonition:: Solution for this step
   :class: hint

   `//pw_async2/codelab/solutions/step4`_

1. Structuring your tasks as state machines
===========================================
The first thing we recommend is explicitly structuring your tasks as state
machines. For the vending machine you might end up with an enum for the states,
and a switch statement in ``DoPend`` that looks like this skeleton:

.. code-block:: cpp

   enum State {
     kWelcome,
     kAwaitingPayment,
     kAwaitingSelection,
   };

   pw::async2::Poll<> VendingMachineTask::DoPend(pw::async2::Context& cx) {
     while (true) {
       switch (state_) {
         case kWelcome: {
           // Show Welcome message
           state_ = kAwaitingPayment;
           break; // Reenter the switch()
         }
         case kAwaitingPayment: {
           // Pend on coin_slot_

           // Once coins are inserted...
           state_ = kAwaitingSelection;
           break; // Reenter the switch()
         }
         case kAwaitingSelection: {
           // Pend on keypad_

           // Once a selection is made
           // Dispense item
           return pw::async2::Ready();
         }
       }
     }
   }

This isn't the only way to do it, but it is perhaps the easiest way to
understand since there isn't a lot of hidden machinery.

Go ahead and convert your implementation to use this pattern, and make sure it
still works.

.. topic:: State machines in C++

   Two other options for implementing a state machine in C++ include:

   - Define a type tag for each state, and use a ``std::variant`` to represent
     the possible states, and ``std::visit`` to dispatch to a handler for each
     of them. Effectively this causes the compiler to generate the switch
     statement for you at compile time.

   - Use runtime dispatch through a function pointer to handle each state.
     Usually you derive each state from a base class that defines a virtual
     function that each state class provides an override for, but there are
     other equivalents.


2. Waiting on multiple pendables
================================
Given that your ``VendingMachine`` has both a ``CoinSlot`` and a ``Keypad``,
there is already another problem in the simple linear flow we've implemented so
far.

You can see it for yourself by pressing :kbd:`1` on the keypad first, and
inserting a coin :kbd:`c` afterwards, followed by :kbd:`Enter`,

What happens in the linear flow, even after you've made the change to use a
state machine pattern?

How do you make your task better at handling multiple inputs when
the ``Pend()`` of ``CoinSlot`` and ``Keypad`` can only wait on one thing?

The answer is to use the :doxylink:`Selector <pw::async2::Selector>` class and
the :doxylink:`Select <pw::async2::Select>` helper function to wait on multiple
pendables, along with the
:doxylink:`VisitSelectResult <pw::async2::VisitSelectResult>` helper that allows
you to unpack the completion result value when one of those pendables returns
``Ready()``

Using ``Selector`` and ``Select``
---------------------------------

- :doxylink:`Select <pw::async2::Select>` is a simple wrapper function for
  ``Selector``. It constructs a temporary instance of the class, and then
  returns the result of calling ``Pend()`` on the instance. The temporary
  instance is then destroyed when the function returns the result.

  This behavior is useful when you have a set of pendables where you want to
  wait on any of them. However take note that this won't ensure each pendable
  has a fair chance to report its stats. The first pendables in the set get
  polled first, and if those are ready, those take precedence.

  Depending on the design of the pendable type, it may also not be possible to
  wait afresh for a new result after the pendable returned ``Ready()``.

- :doxylink:`Selector <pw::async2::Selector>` is a pendable class that polls an
  ordered set of pendables you provide to determine which (if any) are ready.

  If you construct and store a ``Selector`` instance yourself, you can give all
  the pendables in the poll set a chance to return ``Ready()``, since each will
  be polled until the first time it returns ``Ready()``. However you must
  arrange to invoke the ``Pend()`` function on the same ``Selector`` instance
  yourself in a loop.

  Once you process the ``AllPendablesCompleted`` result when using
  ``VisitSelectResult`` (see below), you could then reset the ``Selector`` to once
  again try all the pendables again.


For the vending machine, we'll use ``Select``, so we can wait on multiple keypad
buttons and coins, and respond correctly.

Both ``Select`` and ``Selector`` work by using another helper function
:doxylink:`PendableFor <pw::async2::PendableFor>` to construct a type-erased
wrapper that allows the ``Pend()`` function to be called.

To poll both the ``CoinSlot`` and the ``Keypad``, we use:

.. code-block:: cpp

   PW_TRY_READY_ASSIGN(
       auto result,
       pw::async2::Select(cx,
                          pw::async2::PendableFor<&CoinSlot::Pend>(coin_slot_),
                          pw::async2::PendableFor<&Keypad::Pend>(keypad_)));

We use ``auto`` for the result return type, as the actual type is much more
complicated, and typing out the entire type would be laborious and would not
help with the readability of the code.

As usual, we're using ``PW_TRY_READY_ASSIGN`` so that if all the pendables are
pending then the current function will return ``Pending()``.

If one of those returns ``Ready()``, the ``result`` value will indicate which
one, and will also be holding the value (if any). For the ``CoinSlot`` that
value will be the count of coins added, and for the ``Keypad``, that will be the
button that was pressed.

.. note::

   The result will only contain a single ready result, based on the order of the
   pendables given to the ``Select`` function (or used when constructing the
   ``Selector``). They are checked in the order you give.

   To get them all you may have to make the same call again. Keep in mind that
   with ``Select`` you could see more coin inserts if the ISR for them happens
   to trigger faster than your task can poll for them.

   The bare ``Selector`` does not have that problem, but you will have to reset
   its state instead to see more coin events after the first.


Using ``VisitSelectResult``
---------------------------
:doxylink:`VisitSelectResult <pw::async2::VisitSelectResult>` is a helper for
processing the result of the ``Select`` function or the ``Selector::Pend``
member function call.

The result contains a single ``Ready()`` result, but because of how the result is
stored, there is a bit of C++ template magic to unpack it for each possible
type. ``VisitSelectResult`` does its best to hide most of the details, but you
need to specify an ordered list of lambda functions to handle each specific
pendable result.

.. code-block:: cpp

   pw::async2::VisitSelectResult(
       result,
       [](pw::async2::AllPendablesCompleted) {
         // Special lambda that's only needed when using `Selector::Pend`, and
         // which is invoked when all the other pendables have completed.
         // This can be left blank when using `Select` as it is not used.
       },
       [&](unsigned coins) {
         // This is the first lambda after the `AllPendablesCompleted`` case as
         // `CoinSlot::Pend` was in the first argument to `Select`.
         // Invoked if the `CoinSlot::Pend` is ready, with the count of coins
         // returned as part of the `Poll` result from that call.
       },
       [&](int key) {
         // This is the second lambda after the `AllPendablesCompleted`` case as
         // `Keypad::Pend` was in the second argument to `Select`.
         // Invoked if the `Keypad::Pend` is ready, with the key number
         // returned as part of the `Poll` result from that call.
       });

In case you were curious about the syntax, behind the scenes a ``std::visit`` is
used with a ``std::variant``, and lambdas like these are how you can deal with
the alternative values.

But before you go and use ``Select``, there is one more suggestion.

3. Reusing the ``Select`` code
==============================
Both the ``kAwaitingPayment`` state and the ``kAwaitingSelection`` state are
going to be using the same set of pendables-the ``CoinSlot`` and the ``Keypad``.
As the code involved is template-heavy (leading to lots of compile time code
being generated), it's advisable to encapsulate the calls into a function that
both states can use, without expanding the templates twice.

The best way to do that is to treat input result as a pendable sub-state of the
task's primary state machine.

.. code-block:: cpp

   enum Input {
     kNone,
     kCoinInserted,
     kKeyPressed,
   };

   pw::async2::Poll<Input> VendingMachineTask::PendInput(pw::async2::Context& cx) {
     Input input = kNone;

     PW_TRY_READY_ASSIGN(
         auto result,
         pw::async2::Select(cx,
                            pw::async2::PendableFor<&CoinSlot::Pend>(coin_slot_),
                            pw::async2::PendableFor<&Keypad::Pend>(keypad_)));
     pw::async2::VisitSelectResult(
         result,
         [](pw::async2::AllPendablesCompleted) {},
         [&](unsigned coins) {
           /* do something with coins */
           input = kCoinInserted;
         },
         [&](int key) {
           /* do something with key */
           input = kKeyPressed;
         });

     return input;
   }

Inside the ``kAwaitingPayment`` and ``kAwaitingSelection`` states, you can then
``Pend()`` for and then switch on the input sub-state result:

.. code-block:: cpp

   switch (state_) {

     // …

     case kAwaitingPayment: {
       PW_TRY_READY_ASSIGN(Input input, PendInput(cx));
       switch (input) {
         case kCoinInserted: {
           /* react to the expected coins */
           break;
         }
         case kKeyPressed: {
            /* react to the unexpected input */
            break;
         }
       }
     }

     // And then something similar for the kAwaitingSelection state.
   }

Now go ahead and try filling in the blanks in those snippets. Can you build
something reasonable that handles out-of-order input?

Remember, if you get stuck, you can reference our example solution for this
step: `//pw_async2/codelab/solutions/step4`_

-----------------------------------
Step 5: Communicating between tasks
-----------------------------------
Now that the ``VendingMachineTask`` has been refactored, it's ready to handle
more functionality. In this step, you'll write code to handle the vending
machine's dispenser mechanism. Along the way, you'll learn how to send data
between tasks.

This vending machine uses a motor to push the selected product into a chute. A
sensor detects when the item has dropped, then the motor is turned off.

The dispenser mechanism is complex enough to merit a task of its own. The
``VendingMachineTask`` will send which items to dispense to a new
``DispenserTask``. After dispensing an item, the ``DispenserTask`` will send
confirmation back to the ``VendingMachineTask``.

.. admonition:: Solution for this step
   :class: hint

   `//pw_async2/codelab/solutions/step5`_

1. Set up the ``item_drop_sensor_isr()``
========================================
First, let's set up the item drop sensor. When an item is dispensed
successfully, the item drop sensor triggers an interrupt, which is handled by
the ``item_drop_sensor_isr()`` function.

.. literalinclude:: codelab/hardware.h
   :language: cpp
   :start-at: item_drop_sensor_isr
   :end-at: item_drop_sensor_isr

We've provided an ``ItemDropSensor`` class in
`//pw_async2/codelab/item_drop_sensor.h`_. It is similar to the ``CoinSlot`` and
``Keypad`` classes.

To use it, ``#include "item_drop_sensor.h"`` and declare an ``ItemDropSensor``
instance in your `//pw_async2/codelab/main.cc`_:

.. literalinclude:: codelab/solutions/step5/main.cc
   :language: cpp
   :start-at: codelab::ItemDropSensor
   :end-at: codelab::ItemDropSensor

Then, call it from ``item_drop_sensor_isr()``.

.. literalinclude:: codelab/solutions/step5/main.cc
   :language: cpp
   :start-at: void item_drop_sensor_isr()
   :end-at: void item_drop_sensor_isr()

2. Setting up inter-task communication
======================================
We'll be adding a new ``DispenserTask`` soon. To get ready for that, let's set
up communications channels between ``VendingMachineTask`` and the new task.

There are many ways to use a :doxylink:`Waker <pw::async2::Waker>` to
communicate between tasks. For this step, we'll use
:doxylink:`pw::InlineAsyncQueue` to send events between the two tasks.

.. topic:: ``pw::InlineAsyncQueue`` async member functions

   :doxylink:`pw::InlineAsyncQueue` adds two async member functions to
   :doxylink:`pw::InlineQueue`.

   - :doxylink:`PendHasSpace() <pw::BasicInlineAsyncQueue::PendHasSpace>`:
     Producers call this to ensure the queue has room before producing more
     data.

     .. code-block:: c++

        PW_TRY_READY(async_queue.PendHasSpace(context));
        async_queue.push_back(item);

   - :doxylink:`PendNotEmpty() <pw::BasicInlineAsyncQueue::PendNotEmpty>`:
     Consumers call this to block until data is available to consume.

     .. code-block:: c++

        PW_TRY_READY(async_queue.PendNotEmpty(context));
        Item& item = async_queue.front();
        async_queue.pop();  // Remove the item when done with it.

We'll need two queues, one for each of the following:

- Send dispense requests (item numbers) from the ``VendingMachineTask``
  to the ``DispenserTask``.
- Send dispense responses (success/failure) from the ``DispenserTask``
  to the ``VendingMachineTask``.

For convenience, you can create aliases for these queues in
`//pw_async2/codelab/vending_machine.h`_. A depth of ``1`` is fine for now.

.. literalinclude:: codelab/solutions/step5/vending_machine.h
   :language: cpp
   :start-at: using DispenseRequestQueue =
   :end-at: using DispenseResponseQueue =

Make sure to add ``#include` "pw_containers/inline_async_queue.h"`` to the top
of the file.

Declare a ``dispense_requests`` queue and a ``dispense_response`` queue in your
`//pw_async2/codelab/main.cc`_.

3. Create a new ``DispenserTask``
=================================
The ``DispenserTask`` will turn the dispenser motor on and off in response to
dispense requests from the ``VendingMachineTask``.

Like ``VendingMachineTask``, ``DispenserTask`` will be a state machine. It will
need to handle three states:

- ``kIdle`` -- waiting for a dispense request; motor is off
- ``kDispensing`` -- actively dispensing an item; motor is on
- ``kReportDispenseSuccess`` -- waiting to report success; motor is off
- ``kReportDispenseFailure`` -- waiting to report failure; motor is off

The task will control the vending machine's dispenser motor with the
``SetDispenserMotorState`` function in `//pw_async2/codelab/hardware.h`_.

.. literalinclude:: codelab/hardware.h
   :language: cpp
   :start-at: enum MotorState
   :end-at: SetDispenserMotorState

Declare a ``DispenserTask`` in your `//pw_async2/codelab/vending_machine.h`_
file. It should take references to ``ItemDropSensor`` and the two queues in its
constructor.

.. literalinclude:: codelab/solutions/step5/vending_machine.h
   :language: cpp
   :start-at: class DispenserTask :
   :end-before: pw::async2::TimeFuture
   :append: };

The implementation should be structured as a state machine. You can copy this
stub to your `//pw_async2/codelab/vending_machine.cc`_:

.. code-block:: cpp

   pw::async2::Poll<> DispenserTask::DoPend(pw::async2::Context& cx) {
     // This is a stub implementation!
     while (true) {
       switch (state_) {
         case kIdle: {
           break;
         }
         case kDispensing: {
           break;
         }
         case kReportDispenseSuccess: {
           break;
         }
         case kReportDispenseFailure: {
           break;
         }
       }
     }
   }

Here's what the three states need to do.

- ``kIdle``

  1. Read an item number from the ``dispense_requests_`` queue. This is done by
     calling :doxylink:`PendNotEmpty()
     <pw::BasicInlineAsyncQueue::PendNotEmpty>` and accessing the request with
     a call to ``front()``. Keep the item in the queue until you turn off the
     dispensing motor; you'll need to reference the number.
  2. Call ``item_drop_sensor_.Clear()`` so the sensor is ready for a new item.
  3. Start the motor with a call to ``SetDispenerMotorState()``.
  4. Move to the ``kDispensing`` state.

- ``kDispensing``

  1. Wait for the ``ItemDropSensor`` to trigger with
     ``item_drop_sensor_.Pend()``.
  2. Turn off the dispensing motor, using ``dispense_requests_.front()`` for the
     motor number.
  3. ``pop()`` the dispense request. It's no longer needed since the motor is
     off.
  4. Advance to the ``kReportDispenseSuccess`` state.

- ``kReportDispenseSuccess``

  1. Wait for the response queue to have space with :doxylink:`PendHasSpace()
     <pw::BasicInlineAsyncQueue::PendHasSpace>`.
  2. Signal that the item was dispensed with ``.push(true)``.

     Note that dispensing can't fail at this stage---we'll get to that later.

4. Interact with ``DispenserTask`` from ``VendingMachineTask``
==============================================================
Now, let's get ``VendingMachineTask`` communicating with ``DispenserTask``.

Instead of just logging when a purchase is made, ``VendingMachineTask`` will
send the selected item to the ``DispenserTask`` through the dispense requests
queue. Then it will wait for a response with the dispense responses queue.
Update ``VendingMachineTask``'s constructor to take references to the two
queues.

We'll need two new states in ``VendingMachineTask`` for this:

- ``kAwaitingDispenseIdle`` state.

  This state ensures the ``DispenserTask`` is ready for the request before we
  send it.

  1. Transition to this state after ``kAwaitingSelection``.
  2. Wait for a slot in the dispense requests queue by calling
     :doxylink:`PendHasSpace() <pw::BasicInlineAsyncQueue::PendHasSpace>`.
  3. Push the request to it.
  4. Transition to the new ``kAwaitingDispense`` state.

- ``kAwaitingDispense`` state

  1. Wait for ``DispenserTask`` to report that it finished dispensing the item
     with a call to :doxylink:`PendNotEmpty()
     <pw::BasicInlineAsyncQueue::PendNotEmpty>`.
  2. Display a message with the result.
  3. Return to the ``kWelcome`` state if successful or ``kAwaitingSelection`` if
     not.

5. Build and run: Test the dispenser
====================================
Build and run the codelab, and then press :kbd:`c` :kbd:`Enter` :kbd:`1`
:kbd:`Enter` to input a coin and make a selection.

.. code-block:: sh

   bazelisk run //pw_async2/codelab

   INF  Welcome to the Pigweed Vending Machine!
   INF  Please insert a coin.
   c
   INF  Received 1 coin.
   INF  Please press a keypad key.
   1

You'll notice that the vending machine hasn't finished dispensing the item.
Press :kbd:`i` :kbd:`Enter` to signal that the item has dropped, triggering the
``item_drop_sensor_isr()``. You should see vending machine display its welcome
message again.

Congratulations! You now have a fully functioning vending machine!

6. Handling unexpected situations with timeouts
===============================================
But wait---what if you press the wrong button and accidentally buy an
out-of-stock item? Well, as of now, the dispenser will just keep running
forever. The vending machine will eat your money while you go hungry.

Let's fix this. We can add a timeout to the ``kDispensing`` state. If the
``ItemDropSensor`` hasn't triggered after a certain amount of time, then
something has gone wrong. The ``DispenserTask`` should stop the motor and tell
the ``VendingMachineTask`` what happened.

You can implement a timeout with :doxylink:`pw::async2::TimeFuture`. To use it,
``#include "pw_async2/time_provider.h"`` and declare a ``TimeFuture`` in your
``DispenserTask``.

.. literalinclude:: codelab/solutions/step5/vending_machine.h
   :language: cpp
   :start-at: pw::async2::TimeFuture
   :end-at: pw::async2::TimeFuture
   :dedent:

Define a timeout period in your ``DispenserTask``. For testing purposes, make
sure it's long enough for a human to respond. 5 seconds should do.

.. literalinclude:: codelab/solutions/step5/vending_machine.h
   :language: cpp
   :start-at: kDispenseTimeout =
   :end-at: kDispenseTimeout =
   :dedent:

When you start dispensing an item (in your transition from ``kIdle`` to
``kDispensing``), initialize the :doxylink:`TimeFuture <pw::async2::TimeFuture>`
to your timeout.

.. literalinclude:: codelab/solutions/step5/vending_machine.cc
   :language: cpp
   :start-at: const auto expected_completion
   :end-at: WaitUntil(expected_completion)
   :dedent:

Then, in the ``kDispensing`` state, use :doxylink:`Select <pw::async2::Select>`
to wait for either the timeout or the item drop signal, whichever comes first.
Use :doxylink:`VisitSelectResult <pw::async2::VisitSelectResult>` to take action
based on the result:

.. code-block:: cpp

   pw::async2::VisitSelectResult(
       result,
       [](pw::async2::AllPendablesCompleted) {},
       [&](pw::async2::ReadyType) {
         // Received the item drop interrupt.
         // Note that the type is ReadyType, the type of Ready(). Ready() is an
         // empty placeholder produced by a completed Poll<>.
       },
       [&](std::chrono::time_point<pw::chrono::SystemClock>) {
         // The timeout occurred before the item drop interrupt!
       });

- If the item drop interrupt arrives first, clear the timeout with
  ``timeout_future_ = {}``. If the timer isn't cleared, it will fire later and
  wake ``DispenserTask`` unnecessarily, wasting time and power. After that,
  proceed to the ``kReportDispenseSuccess`` state.
- If the timeout arrives first, proceed to the ``kReportDispenseSuccess`` state.

In either case, be sure to turn off the motor and ``pop()`` the dispense request
from the queue.

7. Build and run: Test the dispenser with timeouts
==================================================
Build and run the codelab, and then press :kbd:`c` :kbd:`Enter` :kbd:`1`
:kbd:`Enter` to input a coin and make a selection.

.. code-block:: sh

   bazelisk run //pw_async2/codelab

The machine will start dispensing, but don't press :kbd:`i`. After the timeout
period, you should see the dispenser time out and ask you to make another
selection.

.. code-block:: text

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

Try again, but this time press :kbd:`i` :kbd:`Enter` quickly so dispensing the
item succeeds.

.. The following references shorten the markup above.

.. _`//pw_async2/codelab/BUILD.bazel`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/BUILD.bazel
.. _`//pw_async2/codelab/coin_slot.cc`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/coin_slot.cc
.. _`//pw_async2/codelab/main.cc`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/main.cc
.. _`//pw_async2/codelab/hardware.h`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/hardware.h
.. _`//pw_async2/codelab/item_drop_sensor.h`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/item_drop_sensor.h
.. _`//pw_async2/codelab/vending_machine.cc`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.cc
.. _`//pw_async2/codelab/vending_machine.h`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/vending_machine.h

.. _`//pw_async2/codelab/solutions/step1`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/solutions/step1
.. _`//pw_async2/codelab/solutions/step2`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/solutions/step2
.. _`//pw_async2/codelab/solutions/step3`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/solutions/step3
.. _`//pw_async2/codelab/solutions/step4`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/solutions/step4
.. _`//pw_async2/codelab/solutions/step5`: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/codelab/solutions/step5
