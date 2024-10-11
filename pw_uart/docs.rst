.. _module-pw_uart:

=======
pw_uart
=======
.. pigweed-module::
   :name: pw_uart

Pigweed's ``pw_uart`` module provides a set of interfaces for communicating via
UART.

--------
Overview
--------
``pw_uart`` defines core methods for UART communication. This serves
as a blueprint for concrete UART implementations. You will need to write the
backend code tailored to your specific hardware device to interact with the
UART peripheral.

The interface consists of these main classes:

- `UartBase`_ - Base class which provides basic enable/disable and
  configuration control, but no communication.
- `Uart`_ - Extends ``pw::uart::UartBase`` to provide blocking Read and Write
  APIs.
- `UartNonBlocking`_ - Extends ``pw::uart::UartBase`` to provide non-blocking
  (callback-based) Read and Write APIs.
- `UartBlockingAdapter`_ - Provides the blocking `Uart`_ interface on top of a
  `UartNonBlocking`_ device.
- `UartStream`_ - Provides the ``pw::stream::NonSeekableReaderWriter``
  (:ref:`module-pw_stream`) interface on top of a `Uart`_ device.

.. mermaid::

   classDiagram
    namespace uart {
      class UartBase
      class Uart
      class UartNonBlocking
      class UartBlockingAdapter
      class UartStream
    }

    namespace stream {
      class NonSeekableReaderWriter
    }

    UartBase <|-- Uart : extends
    UartBase <|-- UartNonBlocking : extends

    Uart <|-- UartBlockingAdapter : implements
    UartNonBlocking --> UartBlockingAdapter

    NonSeekableReaderWriter <|-- UartStream : implements
    Uart --> UartStream

-----------
Get started
-----------
.. repository: https://bazel.build/concepts/build-ref#repositories

.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_uart`` to the ``deps`` list in your Bazel target:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_uart",
             # ...
           ]
         }

      This assumes that your Bazel ``WORKSPACE`` has a `repository
      <https://bazel.build/concepts/build-ref#repositories>`_ named ``@pigweed``
      that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``$dir_pw_uart`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_uart",
             # ...
           ]
         }

   .. tab-item:: CMake

      Add ``pw_uart`` to your ``pw_add_library`` or similar CMake target:

      .. code-block::

         pw_add_library(my_library STATIC
           HEADERS
             ...
           PRIVATE_DEPS
             # ...
             pw_uart
             # ...
         )

.. _module-pw_uart-reference:

-------------
API reference
-------------

.. warning::

   Drivers should not implement both ``Uart`` and ``UartNonBlocking``
   interfaces.

   Drivers which support non-blocking (callback) behavior should implement
   ``UartNonBlocking``. Applications that require the blocking ``Uart``
   interface can use the ``UartBlockingAdapter``.

UartBase
========
.. doxygenclass:: pw::uart::UartBase
  :members:

Uart
====
.. doxygenclass:: pw::uart::Uart
  :members:

UartNonBlocking
===============
.. doxygenclass:: pw::uart::UartNonBlocking
  :members:

UartBlockingAdapter
===================
.. doxygenclass:: pw::uart::UartBlockingAdapter
  :members:

UartStream
==========
.. doxygenclass:: pw::uart::UartStream
  :members:


.. toctree::
   :hidden:
   :maxdepth: 1

   Backends <backends>
