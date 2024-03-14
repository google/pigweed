.. _module-pw_span:

=======
pw_span
=======
.. pigweed-module::
   :name: pw_span

* **Standardized**: :cpp:class:`pw::span` matches C++20's `std::span
  <https://en.cppreference.com/w/cpp/container/span>`_ as closely as possible.
* **Zero-cost**: If ``std::span`` is available, ``pw::span`` is simply an alias
  of it.

.. pw_span-example-start

.. code-block:: cpp

   #include <span>

   // With pw::span, a buffer is passed as a single argument.
   // No need to handle pointer and size arguments.
   bool ProcessBuffer(pw::span<uint8_t> buffer);

   bool DoStuff() {
     // ...
     ProcessBuffer(c_array);
     ProcessBuffer(array_object);
     ProcessBuffer(pw::span(data_pointer, data_size));
   }

.. pw_span-example-end

:cpp:class:`pw::span` is a convenient abstraction that wraps a pointer and a
size. It's especially useful in APIs. Spans support implicit conversions from
C arrays, ``std::array``, or any STL-style container, such as
``std::string_view``.

.. _module-pw_span-start:

-----------
Get started
-----------
.. repository: https://bazel.build/concepts/build-ref#repositories

.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_span`` to the ``deps`` list in your Bazel target:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_span",
             # ...
           ]
         }

      This assumes that your Bazel ``WORKSPACE`` has a `repository
      <https://bazel.build/concepts/build-ref#repositories>`_ named ``@pigweed``
      that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``$dir_pw_span`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_span",
             # ...
           ]
         }

   .. tab-item:: CMake

      Add ``pw_span`` to your ``pw_add_library`` or similar CMake target:

      .. code-block::

         pw_add_library(my_library STATIC
           HEADERS
             ...
           PRIVATE_DEPS
             # ...
             pw_span
             # ...
         )

   .. tab-item:: Zephyr

      There are two ways to use ``pw_span`` from a Zephyr project:

      #. Depend on ``pw_span`` in your CMake target (see CMake tab). This is
         Pigweed Team's suggested approach since it enables precise CMake
         dependency analysis.

      #. Add ``CONFIG_PIGWEED_SPAN=y`` to the Zephyr project's configuration,
         which causes ``pw_span`` to become a global dependency and have the
         includes exposed to all targets. Pigweed team does not recommend this
         approach, though it is the typical Zephyr solution.

------
Guides
------

Operating on arrays of bytes
============================
When operating on an array of bytes you typically need pointer and size
arguments:

.. code-block:: cpp

   bool ProcessBuffer(char* buffer, size_t buffer_size);

   bool DoStuff() {
     ProcessBuffer(c_array, sizeof(c_array));
     ProcessBuffer(array_object.data(), array_object.size());
     ProcessBuffer(data_pointer, data_size);
   }

With ``pw::span`` you can pass the buffer as a single argument instead:

.. include:: docs.rst
   :start-after: .. pw_span-example-start
   :end-before: .. pw_span-example-end

Working with spans of binary data
=================================
Use ``pw::span<std::byte>`` or ``pw::span<const std::byte>`` to represent
spans of binary data. Convert spans to byte spans with ``pw::as_bytes`` or
``pw::as_writable_bytes``.

.. code-block:: cpp

   void ProcessData(pw::span<const std::byte> data);

   void DoStuff() {
     std::array<AnyType, 7> data = { ... };
     ProcessData(pw::as_bytes(pw::span(data)));
   }

``pw_bytes/span.h`` provides ``ByteSpan`` and ``ConstByteSpan`` aliases for
these types.

----------
References
----------

API reference
=============
See `std::span <https://en.cppreference.com/w/cpp/container/span>`_.
The ``pw::span`` API is designed to match the ``std::span`` API.

Configuration options
=====================
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. doxygendefine:: PW_SPAN_ENABLE_ASSERTS
