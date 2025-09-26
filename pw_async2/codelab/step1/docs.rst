.. _module-pw_async2-codelab-step1:

=============
1. Run a task
=============
.. pigweed-module-subpage::
   :name: pw_async2

The two most fundamental components of ``pw_async2`` are tasks and dispatchers.

-------------------------------
Log a welcome message in a task
-------------------------------
:cc:`Task <pw::async2::Task>` is the basic unit of execution in the
``pw_async2`` framework. Tasks are objects that represent jobs to be done, like
blinking an LED, processing sensor data, or running a vending machine.

#. Study the code in ``vending_machine.h``.

   .. literalinclude:: ../vending_machine.h
      :language: cpp
      :start-after: namespace codelab {
      :end-before: }  // namespace codelab
      :linenos:

   ``VendingMachineTask`` inherits from ``Task``.

   The ``DoPend()`` function is where your task will do work.

#. Study the code in ``vending_machine.cc``.

   Here you'll find the incomplete implementation of ``DoPend``.

   .. literalinclude:: ../vending_machine.cc
      :language: cpp
      :start-after: namespace codelab {
      :end-before: }  // namespace codelab
      :linenos:

   The ``DoPend`` method returns a :cc:`Poll\<\> <pw::async2::Poll>`. A
   ``Poll`` can be in one of two states:

   * ``Ready()``: The task has finished its work.
   * ``Pending()``: The task is not yet finished. The dispatcher should run
     it again later.

   Our current ``DoPend()`` immediately returns ``Ready()``, meaning it exits
   without doing any work.

#. Log a welcome message in ``vending_machine.cc``:

   * Log the message at the start of the ``DoPend()`` implementation:

     .. code-block:: cpp

        PW_LOG_INFO("Welcome to the Pigweed Vending Machine!");

     This logging macro comes from :ref:`module-pw_log`. We've already included
     the header that provides this macro (``pw_log/log.h``).

   * Keep the ``Ready()`` return because this tells the dispatcher to complete
     the task after the message has been logged.

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-after: namespace codelab {
         :end-before: }  // namespace codelab
         :linenos:
         :emphasize-lines: 2

-----------------------------
Post the task to a dispatcher
-----------------------------
The :cc:`Dispatcher <pw::async2::Dispatcher>` is the engine that runs the
tasks. It's a simple, cooperative scheduler. You give it tasks by calling
:cc:`Post() <pw::async2::Dispatcher::Post>`. You run tasks with the dispatcher
by calling :cc:`RunUntilStalled() <pw::async2::Dispatcher::RunUntilStalled>`
or :cc:`RunToCompletion() <pw::async2::Dispatcher::RunToCompletion>`.

The dispatcher maintains a queue of tasks that are ready to be polled. When a
run is triggered, it grabs a task from the queue and invokes the task's
``DoPend()`` method. If the task returns ``Pending()``, the task is put to
sleep until it is woken by the operation that blocked it. If the task returns
``Ready()``, the dispatcher considers it complete and will not run the task
again.

#. In ``main.cc``, set up a task and run it with the dispatcher:

   * Create an instance of ``VendingMachineTask``

   * Add the task to the dispatcher's run queue by calling the dispatcher's
     :cc:`Post() <pw::async2::Dispatcher::Post>` method, passing the task as
     an arg

   * Tell the dispatcher to run all of its tasks until they return ``Ready()``
     by calling its :cc:`RunToCompletion()
     <pw::async2::Dispatcher::RunToCompletion>` method

   .. dropdown:: Hint

      .. literalinclude:: ./checkpoint1/main.cc
         :language: cpp
         :start-at: int main()
         :end-at: }
         :emphasize-lines: 5,6,8
         :linenos:

-----------
Run the app
-----------
#. Build and run the app again:

   .. code-block:: console

      bazelisk run //pw_async2/codelab

   You should see the same output as before, in addition to your new welcome
   message:

   .. code-block:: none

      INF  Welcome to the Pigweed Vending Machine!

----------
Next steps
----------
You've written and run your first task with ``pw_async2``. Continue to
:ref:`module-pw_async2-codelab-step2` to learn how to run async operations
in your task.

Check out :ref:`module-pw_async2-informed-poll` to learn more about the
conceptual programming model of ``pw_async2``.

.. _module-pw_async2-codelab-step1-checkpoint:

----------
Checkpoint
----------
At this point, your code should look similar to the files below.

.. tab-set::

   .. tab-item:: main.cc

      .. literalinclude:: ./checkpoint1/main.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.cc

      .. literalinclude:: ./checkpoint1/vending_machine.cc
         :language: cpp
         :start-after: // the License.

   .. tab-item:: vending_machine.h

      .. literalinclude:: ./checkpoint1/vending_machine.h
         :language: cpp
         :start-after: // the License.
