.. _module-pw_work_queue:

=============
pw_work_queue
=============
The ``pw_work_queue`` module contains utilities for deferring work to be
executed by another thread.

.. warning::

   This module is still under construction; the API is not yet stable.

-------
Example
-------

.. code-block:: cpp

   #include "pw_thread/detached_thread.h"
   #include "pw_work_queue/work_queue.h"

   pw::work_queue::WorkQueueWithBuffer<10> work_queue;

   pw::thread::Options& WorkQueueThreadOptions();
   void SomeLongRunningProcessing();

   void SomeInterruptHandler() {
       // Instead of executing the long running processing task in the interrupt,
       // the work_queue executes it on the interrupt's behalf.
       work_queue.CheckPushWork(SomeLongRunningProcessing);
   }

   int main() {
       // Start up the work_queue as a detached thread which runs forever.
       pw::thread::DetachedThread(WorkQueueThreadOptions(), work_queue);
   }

-------------
API reference
-------------
.. doxygennamespace:: pw::work_queue
   :members:
