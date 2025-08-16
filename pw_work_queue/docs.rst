.. _module-pw_work_queue:

=============
pw_work_queue
=============
.. pigweed-module::
   :name: pw_work_queue

The ``pw_work_queue`` module contains utilities for deferring work to be
executed by another thread.

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

.. code-block:: cpp

   #include "pw_thread/detached_thread.h"
   #include "pw_work_queue/work_queue.h"

   struct MyWorkItem {
     int value;
   };

   pw::work_queue::CustomWorkQueueWithBuffer<10, MyWorkItem> work_queue(
      [](MyWorkItem& item) {
        SomeLongRunningProcessing(item);
      });
   pw::thread::Options& WorkQueueThreadOptions();
   void SomeLongRunningProcessing(MyWorkItem& value);

   void SomeInterruptHandler(int value) {
       // Instead of executing the long running processing task in the interrupt,
       // the work_queue executes it on the interrupt's behalf.
       work_queue.CheckPushWork({.value = value});
   }

   int main() {
       // Start up the work_queue as a detached thread which runs forever.
       pw::thread::DetachedThread(WorkQueueThreadOptions(), work_queue);
   }


-------------
API reference
-------------
Moved: :doxylink:`pw_work_queue`
