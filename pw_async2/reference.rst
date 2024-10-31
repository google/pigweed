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

.. doxygenclass:: pw::async2::CoroOrElseTask
  :members:

.. doxygenclass:: pw::async2::Join
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
