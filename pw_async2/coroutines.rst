.. _module-pw_async2-coro:

==========
Coroutines
==========
.. pigweed-module-subpage::
   :name: pw_async2

For projects using C++20, ``pw_async2`` provides first-class support for
coroutines via :doxylink:`pw::async2::Coro`. This allows you to write
asynchronous logic in a sequential, synchronous style, eliminating the need to
write explicit state machines. The ``co_await`` keyword is used to suspend
execution until an asynchronous operation is ``Ready``.

.. code-block:: cpp

   Coro<Status> ReadAndSend(Reader& reader, Writer& writer) {
     // co_await suspends the coroutine until the Read operation completes.
     Result<Data> data = co_await reader.Read();
     if (!data.ok()) {
       co_return data.status();
     }

     // The coroutine resumes here and continues.
     co_await writer.Write(*data);
     co_return OkStatus();
   }

See also :ref:`docs-blog-05-coroutines`, a blog post on how Pigweed implements
coroutines without heap allocation, and challenges encountered along the way.

.. _module-pw_async2-coro-tasks:

------------
Define tasks
------------
The following code example demonstrates basic usage:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-coro]
   :end-before: [pw_async2-examples-basic-coro]

Any value with a ``Poll<T> Pend(Context&)`` method can be passed to
``co_await``, which will return with a ``T`` when the result is ready. The
:doxylink:`pw::async2::PendFuncAwaitable` class can also be used to
``co_await`` on a provided delegate function.

To return from a coroutine, ``co_return <expression>`` must be used instead of
the usual ``return <expression>`` syntax. Because of this, the
:c:macro:`PW_TRY` and :c:macro:`PW_TRY_ASSIGN` macros are not usable within
coroutines. :c:macro:`PW_CO_TRY` and :c:macro:`PW_CO_TRY_ASSIGN` should be
used instead.

For a more detailed explanation of Pigweed's coroutine support, see
:doxylink:`pw::async2::Coro`.

------
Memory
------
When using C++20 coroutines, the compiler generates code to save the
coroutine's state (including local variables) across suspension points
(``co_await``). ``pw_async2`` hooks into this mechanism to control where this
state is stored.

A :doxylink:`pw::async2::CoroContext`, which holds a
:doxylink:`pw::Allocator`, must be passed to any function that
returns a :doxylink:`pw::async2::Coro`. This allocator is used to allocate the
coroutine frame. If allocation fails, the resulting ``Coro`` will be invalid
and will immediately return a ``Ready(Status::Internal())`` result when polled.
This design makes coroutine memory usage explicit and controllable.

.. _module-pw_async2-coro-queue:

----------------------------------------------------
Example: using InlineAsyncQueue and InlineAsyncDeque
----------------------------------------------------
If you choose to use C++20 coroutines, you can also use an async2 dispatcher,
as well as awaiting on the pendable interface for the queue.

The following example can be built and run in upstream Pigweed with the
following command:

.. code-block:: sh

   bazelisk run //pw_async2/examples:inline-async-queue-with-coro --config=cxx20

The complete code can be found here:

.. _//pw_async2/examples/inline_async_queue_with_coro_test.cc: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/inline_async_queue_with_coro_test.cc

* `//pw_async2/examples/inline_async_queue_with_coro_test.cc`_

The C++ code simulates a producer and consumer task setup, where the producer
writes to the queue, and the consumer reads it. For purposes of this example,
the data is just integers, with a fixed sequence sent by the producer.

To start with, here are the basic declarations for the queue and a special
terminal sentinel value.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-declarations]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-declarations]

To use the :doxylink:`PendHasSpace
<pw::containers::internal::AsyncCountAndCapacity::PendHasSpace>`, and
:doxylink:`PendNotEmpty
<pw::containers::internal::AsyncCountAndCapacity::PendNotEmpty>` functions with
``co_await``, we need to use :doxylink:`pw::async2::PendFuncAwaitable` as an
adapter between the async2 polling system and the C++20 coroutine framework.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-adapters]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-adapters]

The producer coroutine just needs to return a ``Coro<Status>`` to turn
it into a coroutine, and to use the ``QueueHasSpace`` adapter we define
to wait for there to be space in the queue. Once it is done, it should
``co_return`` a status value to indicate it is complete.

Compare this to the inline_async_queue_with_task.cc example, where the
``Producer::DoPend`` function has to be written in a way that allows
the function to be called fresh at any time, and has to figure out what it
should do next.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-producer]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-producer]

The consumer coroutine similarly needs to return a ``Coro<Status>``
value, and to use the ``QueueNotEmpty`` adapter we define to wait there
to be content in the queue. Once it is done, it should ``co_return`` a status
value to indicate it is complete.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-consumer]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-consumer]

At that point, it is straightforward to set up the dispatcher to run the two
coroutines. Notice however that the :doxylink:`pw::async2::CoroContext` also
needs to allocate memory dynamically when the coroutine is first created. For
this example, we use :doxylink:`LibCAllocator <pw::allocator::LibCAllocator>`.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-run]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-run]

Running the example should produce the following output.

.. literalinclude:: examples/inline_async_queue_with_coro_test.expected
   :start-after: [ RUN      ] ExampleTests.InlineAsyncQueueWithCoro
   :end-before: [       OK ] ExampleTests.InlineAsyncQueueWithCoro

Notice how the producer fills up the queue with four values, then the consumer
gets a chance to empty the queue before the writer gets another chance to run.
