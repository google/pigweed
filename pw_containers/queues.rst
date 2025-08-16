.. _module-pw_containers-queues:

======
Queues
======
.. pigweed-module-subpage::
   :name: pw_containers

A queue is an ordered collection designed to add items at one end and remove
them from the other. This allows "first in, first out", or FIFO, behavior.
Pigweed provides both single and double-ended queues that are backed by fixed
or dynamic storage.

Pigweed provides many queue and deque implementations to meet different needs.

-------------
API reference
-------------
Moved: :doxylink:`pw_containers_queues`

.. _module-pw_containers-inlinevarlenentryqueue:

--------------------------
pw::InlineVarLenEntryQueue
--------------------------
:doxylink:`InlineVarLenEntryQueue` is a queue of inline variable-length binary
entries. It is implemented as a ring (circular) buffer and supports operations
to append entries and overwrite if necessary. Entries may be zero bytes up to
the maximum size supported by the queue.

``InlineVarLenEntryQueue`` has a few interesting properties:

- Data and metadata are stored inline in a contiguous block of
  ``uint32_t``-aligned memory.
- The data structure is trivially copyable.
- All state changes are accomplished with a single update to a ``uint32_t``.
  The memory is always in a valid state and may be parsed offline.

This data structure is a much simpler version of ``PrefixedEntryRingBuffer``.
Prefer this sized-entry ring buffer to ``PrefixedEntryRingBuffer`` when:

- A simple ring buffer of variable-length entries is needed. Advanced
  features like multiple readers and a user-defined preamble are not
  required.
- A consistent, parsable, in-memory representation is required (e.g. to
  decode the buffer from a block of memory).
- C support is required.

``InlineVarLenEntryQueue`` is implemented in C and provides complete C and C++
APIs. The ``InlineVarLenEntryQueue`` C++ class is structured similarly to
:doxylink:`pw::InlineQueue` and :doxylink:`pw::Vector`.

Queue vs. deque
===============
This module provides
:ref:`module-pw_containers-inlinevarlenentryqueue`, but no
corresponding ``InlineVarLenEntryDeque`` class. Following the C++ Standard
Library style, the deque class would provide ``push_front()`` and ``pop_back()``
operations in addition to ``push_back()`` and ``pop_front()`` (equivalent to a
queue's ``push()`` and ``pop()``).

There is no ``InlineVarLenEntryDeque`` class because there is no efficient way
to implement ``push_front()`` and ``pop_back()``. These operations would
necessarily be ``O(n)``, since each entry knows the position of the next entry,
but not the previous, as in a single-linked list. Given that these operations
would be inefficient and unlikely to be used, they are not implemented, and only
a queue class is provided.

Example
=======
.. tab-set::

   .. tab-item:: C++
      :sync: c++

      Queues are declared with their max size
      (``InlineVarLenEntryQueue<kMaxSize>``) but may be used without
      specifying the size (``InlineVarLenEntryQueue<>&``).

      .. code-block:: c++

         // Declare a queue with capacity sufficient for one 10-byte entry or
         // multiple smaller entries.
         pw::InlineVarLenEntryQueue<10> queue;

         // Push an entry, asserting if the entry does not fit.
         queue.push(queue, data)

         // Use push_overwrite() to push entries, overwriting older entries
         // as needed.
         queue.push_overwrite(queue, more_data)

         // Remove an entry.
         queue.pop();

      Alternately, a ``InlineVarLenEntryQueue`` may be initialized in an
      existing ``uint32_t`` array.

      .. code-block:: c++

         // Initialize a InlineVarLenEntryQueue.
         uint32_t buffer[32];
         auto& queue = pw::InlineVarLenEntryQueue<>::Init(buffer);

         // Largest supported entry is 114 B (13 B overhead + 1 B prefix)
         assert(queue.max_size_bytes() == 114u);

         // Write data
         queue.push_overwrite(data);

   .. tab-item:: C
      :sync: c

      A ``InlineVarLenEntryQueue`` may be declared and initialized in C with the
      :c:macro:`PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE` macro.

      .. code-block:: c

         // Declare a queue with capacity sufficient for one 10-byte entry or
         // multiple smaller entries.
         PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(queue, 10);

         // Push an entry, asserting if the entry does not fit.
         pw_InlineVarLenEntryQueue_Push(queue, "12345", 5);

         // Use push_overwrite() to push entries, overwriting older entries
         // as needed.
         pw_InlineVarLenEntryQueue_PushOverwrite(queue, "abcdefg", 7);

         // Remove an entry.
         pw_InlineVarLenEntryQueue_Pop(queue);

      Alternately, a ``InlineVarLenEntryQueue`` may be initialized in an
      existing ``uint32_t`` array.

      .. code-block:: c

         // Initialize a InlineVarLenEntryQueue.
         uint32_t buffer[32];
         pw_InlineVarLenEntryQueue_Init(buffer, 32);

         // Largest supported entry is 114 B (13 B overhead + 1 B prefix)
         assert(pw_InlineVarLenEntryQueue_MaxSizeBytes(buffer) == 114u);

         // Write some data
         pw_InlineVarLenEntryQueue_PushOverwrite(buffer, "123", 3);

Python API reference
====================
.. automodule:: pw_containers.inline_var_len_entry_queue
   :members:

------------
Size reports
------------
The tables below illustrate the following scenarios:

.. TODO: b/394341806 - Add size report for InlineVarLenEntryQueue.

* Scenarios related to ``std::deque``, as a baseline:

  * The memory and code size cost incurred by a adding a single ``std::deque``.
  * The memory and code size cost incurred by adding another ``std::deque``
    with a different type. As ``std::deque`` is templated on type, this
    results in additional code being generated.

* Scenarios related to ``DynamicDeque``:

  * The memory and code size cost incurred by a adding a single ``DynamicDeque``.
  * The memory and code size cost incurred by adding another ``DynamicDeque``
    with a different type. As ``DynamicDeque`` is templated on type, this
    results in additional code being generated.

* Scenarios related to ``InlineDeque``:

  * The memory and code size cost incurred by a adding a single ``InlineDeque``.
  * The memory and code size cost incurred by adding another ``InlineDeque``
    with a different type. As ``InlineDeque`` is templated on type, this
    results in additional code being generated.

* Scenarios related to ``InlineQueue``:

  * The memory and code size cost incurred by a adding a single ``InlineQueue``.
  * The memory and code size cost incurred by adding another ``InlineQueue``
    with a different type. As ``InlineQueue`` is templated on type, this results
    in additional code being generated.

* The memory and code size cost incurred by adding both an ``InlineDeque`` and
  an ``InlineQueue`` of the same type. These types reuse code, so the combined
  sum is less than the sum of its parts.

.. include:: queues_size_report
