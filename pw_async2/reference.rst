.. _module-pw_async2-reference:

=========
Reference
=========
.. pigweed-module-subpage::
   :name: pw_async2

.. _module-pw_async2-reference-cpp:

-----------------
C++ API reference
-----------------
.. TODO: https://pwbug.dev/376082608 - Remove ``:private-members:``.

.. doxygenclass:: pw::async2::Task
  :members:
  :private-members:

.. doxygenclass:: pw::async2::Poll
  :members:

.. doxygenfunction:: pw::async2::Ready()

.. doxygenfunction:: pw::async2::Ready(std::in_place_t, Args&&... args)

.. doxygenfunction:: pw::async2::Ready(T&& value)

.. doxygenfunction:: pw::async2::Pending()

.. doxygenclass:: pw::async2::Context
  :members:

.. doxygenclass:: pw::async2::Waker
  :members:

.. c:macro:: PW_ASYNC_STORE_WAKER

  Arguments: ``Context& cx, Waker& waker_out, StringLiteral wait_reason_string``

  Stores a waker associated with the current context in ``waker_out``.
  When ``waker_out`` is later awoken with :cpp:func:`pw::async2::Waker::Wake`,
  the :cpp:class:`pw::async2::Task` associated with ``cx`` will be awoken and
  its ``DoPend`` method will be invoked again.

.. c:macro:: PW_ASYNC_CLONE_WAKER

  Arguments: ``Waker& waker_in, Waker& waker_out, StringLiteral wait_reason_string``

  Stores a waker associated with ``waker_in`` in ``waker_out``.
  When ``waker_out`` is later awoken with :cpp:func:`pw::async2::Waker::Wake`,
  the :cpp:class:`pw::async2::Task` associated with ``waker_in`` will be awoken
  and its ``DoPend`` method will be invoked again.

.. doxygenclass:: pw::async2::Dispatcher
  :members:

.. doxygenclass:: pw::async2::Coro
  :members:

.. doxygenclass:: pw::async2::CoroContext
  :members:

.. doxygenclass:: pw::async2::TimeProvider
   :members:

.. doxygenfunction:: pw::async2::GetSystemTimeProvider

.. doxygenclass:: pw::async2::SimulatedTimeProvider
   :members:

.. _module-pw_async2-reference-cpp-utilities:

Utilities
=========
.. doxygenfunction:: pw::async2::EnqueueHeapFunc

.. doxygenfunction:: pw::async2::AllocateTask(pw::allocator::Allocator& allocator, Pendable&& pendable)

.. doxygenfunction:: pw::async2::AllocateTask(pw::allocator::Allocator& allocator, Args&&... args)

.. doxygengroup:: pw_async2_pendable_for
   :content-only:

.. doxygenclass:: pw::async2::CoroOrElseTask
  :members:

.. doxygenclass:: pw::async2::Join
  :members:

.. doxygenclass:: pw::async2::Selector
  :members:

.. doxygenstruct:: pw::async2::SelectResult
  :members:

.. doxygenfunction:: pw::async2::VisitSelectResult

.. doxygenstruct:: pw::async2::AllPendablesCompleted
  :members:

.. doxygenfunction:: pw::async2::Select

.. doxygenclass:: pw::async2::PendFuncAwaitable
  :members:

.. doxygenclass:: pw::async2::PendFuncTask
  :members:

.. doxygenclass:: pw::async2::PendableAsTask
  :members:

.. doxygenfunction:: pw::async2::MakeOnceSenderAndReceiver

.. doxygenclass:: pw::async2::OnceSender
  :members:

.. doxygenclass:: pw::async2::OnceReceiver
  :members:

.. doxygenfunction:: pw::async2::MakeOnceRefSenderAndReceiver

.. doxygenclass:: pw::async2::OnceRefSender
  :members:

.. doxygenclass:: pw::async2::OnceRefReceiver
  :members:

.. doxygenclass:: pw::async2::WakerQueue
  :members:

Configuration options
=====================
The following options can be adjusted via compile-time configuration of
``pw_async2``, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. doxygenfile:: pw_async2/public/pw_async2/internal/config.h
   :sections: define

Debugging
=========
Tasks registered to a dispatcher can be inspected by calling
``Dispatcher::LogRegisteredTasks()``, which outputs logs for each task in the
dispatcher's pending and sleeping queues.

Sleeping tasks will log information about their assigned wakers, with the
wait reason provided for each.

If space is a concern, the module configuration option
``PW_ASYNC2_DEBUG_WAIT_REASON`` can be set to ``0``, disabling wait reason
storage and logging. Under this configuration, only the waker count of a
sleeping task is logged.
