.. _module-pw_span:

-------
pw_span
-------
The ``pw_span`` module provides an implementation of C++20's
`std::span <https://en.cppreference.com/w/cpp/container/span>`_, which is a
non-owning view of an array of values. The intent is for this implementation of
``std::span`` is to exactly match the C++20 standard.

The only header provided by the ``pw_span`` module is ``<span>``. It is included
as if it were coming from the C++ Standard Library. If the C++ library provides
``<span>``, the library's version of ``std::span`` is used in place of
``pw_span``'s.

``pw_span`` requires two include paths -- ``public/`` and ``public_overrides/``.
The internal implementation header is in ``public/``, and the ``<span>`` header
that mimics the C++ Standard Library is in ``public_overrides/``.

Using std::span
===============
``std::span`` is a convenient abstraction that wraps a pointer and a size.
``std::span`` is especially useful in APIs. Spans support implicit conversions
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

Pointer and size arguments can be replaced with a ``std::span``:

.. code-block:: cpp

  #include <span>

  // With std::span, the buffer is passed as a single argument.
  bool ProcessBuffer(std::span<uint8_t> buffer);

  bool DoStuff() {
    ProcessBuffer(c_array);
    ProcessBuffer(array_object);
    ProcessBuffer(std::span(data_pointer, data_size));
  }

.. tip::
  Use ``std::span<std::byte>`` or ``std::span<const std::byte>`` to represent
  spans of binary data. Use ``std::as_bytes`` or ``std::as_writeable_bytes``
  to convert any span to a byte span.

  .. code-block:: cpp

    void ProcessData(std::span<const std::byte> data);

    void DoStuff() {
      std::array<AnyType, 7> data = { ... };
      ProcessData(std::as_bytes(std::span(data)));
    }

Compatibility
=============
Works with C++11, but some features require C++17.
