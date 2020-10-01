.. _module-pw_stream:

---------
pw_stream
---------

``pw_stream`` provides a foundational interface for streaming data from one part
of a system to another. In the simplest use cases, this is basically a memcpy
behind a reusable interface that can be passed around the system. On the other
hand, the flexibility of this interface means a ``pw_stream`` could terminate is
something more complex, like a UART stream or flash memory.

Overview
========
At the most basic level, ``pw_stream``'s interfaces provide very simple handles
to enabling streaming data from one location in a system to an endpoint.

Example:

.. code-block:: cpp

  void DumpSensorData(pw::stream::Writer& writer) {
    static char temp[64];
    ImuSample imu_sample;
    imu.GetSample(&info);
    size_t bytes_written = imu_sample.AsCsv(temp, sizeof(temp));
    writer.Write(temp, bytes_written);
  }

In this example, ``DumpSensorData()`` only cares that it has access to a
``Writer`` that it can use to stream data to using ``Writer::Write()``. The
``Writer`` itself can be backed by anything that can act as a data "sink."


pw::stream::Writer
------------------
This is the foundational stream ``Writer`` abstract class. Any class that wishes
to implement the ``Writer`` interface **must** provide a ``DoWrite()``
implementation. Note that ``Write()`` itself is **not** virtual, and should not
be overridden.

Buffering
^^^^^^^^^
If any buffering occurs in a ``Writer`` and data must be flushed before it is
fully committed to the sink, a ``Writer`` implementation is resposible for any
``Flush()`` capability.

pw::stream::Reader
------------------
This is the foundational stream ``Reader`` abstract class. Any class that wishes
to implement the ``Reader`` interface **must** provide a ``DoRead()``
implementation. Note that ``Read()`` itself is **not** virtual, and should not
be overridden.

pw::stream::MemoryWriter
------------------------
The ``MemoryWriter`` class implements the ``Writer`` interface by backing the
data destination with an **externally-provided** memory buffer.
``MemoryWriterBuffer`` extends ``MemoryWriter`` to internally provide a memory
buffer.

pw::stream::MemoryReader
------------------------
The ``MemoryReader`` class implements the ``Reader`` interface by backing the
data source with an **externally-provided** memory buffer.

pw::stream::NullWriter
------------------------
The ``NullWriter`` class implements the ``Writer`` interface by dropping all
requested data writes, similar to ``/dev/null``.

Why use pw_stream?
==================

Standard API
------------
``pw_stream`` provides a standard way for classes to express that they have the
ability to write data. Writing to one sink versus another sink is a matter of
just passing a reference to the appropriate ``Writer``.

As an example, imagine dumping sensor data. If written against a random HAL
or one-off class, there's porting work required to write to a different sink
(imagine writing over UART vs dumping to flash memory). Building a "dumping"
implementation against the ``Writer`` interface prevents a dependency from
forming on an artisainal API that would require porting work.

Similarly, after building a ``Writer`` implementation for a Sink that data
could be dumped to, that same ``Writer`` can be reused for other contexts that
already write data to the ``pw::stream::Writer`` interface.

Before:

.. code-block:: cpp

  // Not reusable, depends on `Uart`.
  void DumpSensorData(Uart& uart) {
    static char temp[64];
    ImuSample imu_sample;
    imu.GetSample(&info);
    size_t bytes_written = imu_sample.AsCsv(temp, sizeof(temp));
    uart.Transmit(temp, bytes_written, /*timeout_ms=*/ 200);
  }

After:

.. code-block:: cpp

  // Reusable; no more Uart dependency!
  void DumpSensorData(Writer& writer) {
    static char temp[64];
    ImuSample imu_sample;
    imu.GetSample(&info);
    size_t bytes_written = imu_sample.AsCsv(temp, sizeof(temp));
    writer.Write(temp, bytes_written);
  }

Reduce intermediate buffers
---------------------------
Often functions that write larger blobs of data request a buffer is passed as
the destination that data should be written to. This *requires* a buffer is
allocated, even if the data only exists in that buffer for a very short period
of time before it's  written somewhere else.

In situations where data read from somewhere will immediately be written
somewhere else, a ``Writer`` interface can cut out the middleman buffer.

Before:

.. code-block:: cpp

  // Requires an intermediate buffer to write the data as CSV.
  void DumpSensorData(Uart* uart) {
    char temp[64];
    ImuSample imu_sample;
    imu.GetSample(&info);
    size_t bytes_written = imu_sample.AsCsv(temp, sizeof(temp));
    uart.Transmit(temp, bytes_written, /*timeout_ms=*/ 200);
  }

After:

.. code-block:: cpp

  // Both DumpSensorData() and RawSample::AsCsv() use a Writer, eliminating the
  // need for an intermediate buffer.
  void DumpSensorData(Writer* writer) {
    RawSample imu_sample;
    imu.GetSample(&info);
    imu_sample.AsCsv(writer);
  }

Prevent buffer overflow
-----------------------
When copying data from one buffer to another, there must be checks to ensure the
copy does not overflow the destination buffer. As this sort of logic is
duplicated throughout a codebase, there's more opportunities for bound-checking
bugs to sneak in. ``Writers`` manage this logic internally rather than pushing
the bounds checking to the code that is moving or writing the data.

Similarly, since only the ``Writer`` has access to any underlying buffers, it's
harder for functions that share a ``Writer`` to accidentally clobber data
written by others using the same buffer.

Before:

.. code-block:: cpp

  Status BuildPacket(Id dest, span<const std::byte> payload,
                     span<std::byte> dest) {
    Header header;
    if (dest.size_bytes() + payload.size_bytes() < sizeof(Header)) {
      return Status::RESOURCE_EXHAUSTED;
    }
    header.dest = dest;
    header.src = DeviceId();
    header.payload_size = payload.size_bytes();

    memcpy(dest.data(), &header, sizeof(header));
    // Forgetting this line would clobber buffer contents. Also, using
    // a temporary span instead could leave `dest` to be misused elsewhere in
    // the function.
    dest = dest.subspan(sizeof(header));
    memcpy(dest.data(), payload.data(), payload.size_bytes());
  }

After:

.. code-block:: cpp

  Status BuildPacket(Id dest, span<const std::byte> payload, Writer& writer) {
    Header header;
    header.dest = dest;
    header.src = DeviceId();
    header.payload_size = payload.size_bytes();

    writer.Write(header);
    return writer.Write(payload);
  }

Why NOT pw_stream?
==================
pw_stream provides a virtual interface. This inherently has more overhead than
a regular function call. In extremely performance-sensitive contexts, a virtual
interface might not provide enough utility to justify the performance cost.

Dependencies
============
  * ``pw_assert`` module
  * ``pw_preprocessor`` module
  * ``pw_status`` module
  * ``pw_span`` module
