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

----------------
pw::DynamicDeque
----------------
.. doxygenclass:: pw::DynamicDeque
   :members:
   :undoc-members:

---------------
pw::InlineDeque
---------------
.. doxygentypedef:: pw::InlineDeque

.. TODO: b/394341806 - Add missing examples
.. Example
.. =======
.. .. literalinclude:: examples/inline_deque.cc
..    :language: cpp
..    :linenos:
..    :start-after: [pw_containers-inline_deque]
..    :end-before: [pw_containers-inline_deque]

API reference
=============
.. doxygenclass:: pw::BasicInlineDeque
   :members:

----------------
pw::DynamicQueue
----------------
.. doxygenclass:: pw::DynamicQueue
   :members:
   :undoc-members:

---------------
pw::InlineQueue
---------------
.. doxygentypedef:: pw::InlineQueue

.. TODO: b/394341806 - Add missing examples
.. Example
.. =======
.. .. literalinclude:: examples/inline_deque.cc
..    :language: cpp
..    :linenos:
..    :start-after: [pw_containers-inline_deque]
..    :end-before: [pw_containers-inline_deque]

API reference
=============
.. doxygenclass:: pw::BasicInlineQueue
   :members:

.. _module-pw_containers-queues-inline_var_len_entry_queue:

--------------------------
pw::InlineVarLenEntryQueue
--------------------------
.. doxygenfile:: pw_containers/inline_var_len_entry_queue.h
   :sections: detaileddescription

.. TODO: b/394341806 - Move code to compiled examples

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

API reference
=============
C++
---
.. doxygengroup:: inline_var_len_entry_queue_cpp_api
   :content-only:
   :members:

C
-
.. doxygengroup:: inline_var_len_entry_queue_c_api
   :content-only:

Python
------
.. automodule:: pw_containers.inline_var_len_entry_queue
   :members:

Queue vs. deque
===============
This module provides
:ref:`module-pw_containers-queues-inline_var_len_entry_queue`, but no
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

Size reports
------------
The tables below illustrate the following scenarios:

.. TODO: b/394341806 - Add size report for InlineVarLenEntryQueue.

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

* The memory and code size cost incurred by a adding both an ``InlineDeque`` and
  an ``InlineQueue`` of the same type. These types reuse code, so the combined
  sum is less than the sum of its parts.

.. include:: queues_size_report
