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
like coin insertion and keypad presses before dispensing an item. Along the way,
you'll learn the core concepts of ``pw_async2``.

By the end of this codelab, you will know how to:

*   Implement a ``pw::async2::Task`` as a state machine.
*   Call asynchronous functions and manage state across suspension points.
*   Write your own pendable functions that use a ``Waker`` to handle external
    events.
*   Use ``pw::async2::OnceSender`` and ``pw::async2::OnceReceiver`` for basic
    inter-task communication.
*   Use ``pw::async2::TimeProvider`` and ``pw::async2::Select`` to implement
    timeouts.

The code for this codelab is part of the Pigweed repository. If you haven't
already, follow the `contributor guide <https://pigweed.dev/contributing/#clone-the-repo>`_
to clone the Pigweed repository and set up your development environment.

Let's get started!

Step 1: Hello, Async World!
===========================
The first step is to create and run a basic asynchronous task. This will
introduce you to the two most fundamental components of ``pw_async2``: the
**Task** and the **Dispatcher**.

What's a Task?
---------------
A ``pw::async2::Task`` is the basic unit of execution in this framework. It's an
object that represents a job to be done, like blinking an LED, processing sensor
data, or, in our case, running a vending machine.

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

The ``DoPend`` method returns a ``pw::async2::Poll<>``. A ``Poll`` can be in one
of two states:

*   ``Ready()``: The task has finished its work.
*   ``Pending()``: The task is not yet finished and should be run again later.

Our stub currently simply returns ``Ready()``, meaning it would exit immediately
without doing any work.

What's a Dispatcher?
--------------------
A ``pw::async2::Dispatcher`` is the engine that runs the tasks. It's a simple,
cooperative scheduler. You give it tasks by calling ``Post()``, and then you
tell it to run them by calling ``RunUntilStalled()`` or ``RunToCompletion()``.

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
-----------------------
Now, let's modify the code to print a welcome message.

4. Implement ``DoPend``

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
