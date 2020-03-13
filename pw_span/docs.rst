.. _chapter-pw-span:

.. default-domain:: cpp

.. highlight:: sh

-------
pw_span
-------
The ``pw_span`` module provides an implementation of C++20's
`std::span <https://en.cppreference.com/w/cpp/container/span>`_, which is a
non-owning view of an array of values. The intent is for ``pw::span``'s
interface to exactly match ``std::span``.

``pw::span`` is a convenient abstraction that wraps a pointer and a size.
``pw::span`` is especially useful in APIs. Spans support implicit conversions
from C arrays, ``std::array``, or any STL-style container, such as
``std::string_view``.

Functions operating on an array of bytes typically accept pointer and size
arguments:

.. code-block:: cpp

  bool ProcessBuffer(char* buffer, size_t buffer_size);

  bool DoStuff() {
    ProcessBuffer(c_array, sizeof(c_array));
    ProcessBuffer(array_object.data(), array_object.size());
    ProcessBuffer(data_pointer, data_size);
  }

Pointer and size arguments can be replaced with a ``pw::span``:

.. code-block:: cpp

  // With pw::span, the buffer is passed as a single argument.
  bool ProcessBuffer(const pw::span<uint8_t>& buffer);

  bool DoStuff() {
    ProcessBuffer(c_array);
    ProcessBuffer(array_object);
    ProcessBuffer(pw::span(data_pointer, data_size));
  }

.. tip::
  Use ``pw::span<std::byte>`` or ``pw::span<const std::byte>`` to represent
  spans of binary data. Use ``pw::as_bytes`` or ``pw::as_writeable_bytes``
  to convert any span to a byte span.

  .. code-block:: cpp

    void ProcessData(pw::span<const std::byte> data);

    void DoStuff() {
      std::array<AnyType, 7> data = { ... };
      ProcessData(pw::as_bytes(pw::span(data)));
    }

Compatibility
=============
C++17
