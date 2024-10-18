.. _module-pw_channel:

==========
pw_channel
==========
.. pigweed-module::
   :name: pw_channel

.. cpp:namespace:: pw::channel

-------------
Why channels?
-------------
``pw_channel`` provides features that are essential for efficient,
high-performance communications.

Flow control
============
Flow control is managing the flow of data between two nodes. If one node sends
more data than the other node can receive, network performance degrades. The
effects vary, but could include data loss, throughput drops, or even crashes in
one or both nodes.

In networking, backpressure is when a node, overwhelmed by inbound traffic,
exterts pressure on upstream nodes. Flow control schemes must account for
backpressure. Communications APIs have to provide ways to release the pressure,
allowing higher layers to can reduce data rates or drop stale or unnecessary
packets. Without this, network communications may break down.

Expressing backpressure in an API might seem simple. You could just return a
status code that indicates that the link isn't ready, and retry when it is.

..  code-block:: cpp

   // Returns `UNAVAILABLE` if the link isn't ready for data yet; retry later.
   Status Write(std::span<const std::byte> packet);

In practice, this is very difficult to work with:

..  code-block:: cpp

    std::span packet = ExpensiveOperationToPreprarePacket();
    if (Write(packet).IsUnavailable()) {
      // ... then what?
    }

Now what do you do? You did work to prepare the packet, but you can't send it.
Do you store the packet somewhere and retry? Or, wait a bit and recreate the
packet, then try to send again? How long do you wait before sending?

The result is inefficient code that is difficult to write correctly.

There are other options: you can add an ``IsReadyToWrite()`` function, and only
call ``Write`` when that is true. But what if ``IsReadyToWrite()`` becomes false
while you're preparing the packet? Then, you're back in the same situation.
Another approach: block until the link is ready for a write. But this means an
entire thread and its resources are locked up for an arbitrary amount of time.

``pw_channel`` solves for backpressure. The implementation can exert
backpressure in each of these places:

- :cpp:func:`GetWriteAllocator <pw::channel::AnyChannel::GetWriteAllocator>` --
  If used, the channel's allocator only provides memory when the channel is
  ready.
- :cpp:func:`PendReadyToWrite <pw::channel::AnyChannel::PendReadyToWrite>` --
  The channel wakes the task when it is ready to accept outbound data. The task
  then calls :cpp:func:`Write <pw::channel::AnyChannel::Write>`, which is
  guaranteed to succeed.  :cpp:func:`Write <pw::channel::AnyChannel::Write>`
  takes ownership of the memory.
- :cpp:func:`PendFlush <pw::channel::AnyChannel::PendFlush>` commits the write.
  Wakes the task only when there are more writes to commit.

The task sending data can do other work while the channel is busy, and it
doesn't have to prepare the packet until the channel is ready to receive it.

------------------
Channel attributes
------------------
Channels may be reliable, readable, writable, or seekable. A channel may be
substituted for another as long as it provides at least the same set of
capabilities; additional capabilities are okay. The channel's data type
(datagram or byte) implies different read/write semantics, so datagram/byte
channels cannot be used interchangeably in general.

Using datagram channels as byte channels
========================================
For datagram channels, the exact bytes provided to a write call will appear in a
read call on the other end. A zero-byte datagram write results in a zero-byte
datagram read, so empty datagrams may convey information.

For byte channels, bytes written may be grouped differently when read. A
zero-length byte write is meaningless and will not result in a zero-length byte
read. If a zero-length byte read occurs, it is ignored.

To facilitate simple code reuse, datagram-oriented channels may used as
byte-oriented channels when appropriate. Calling
:cpp:func:`Channel::IgnoreDatagramBoundaries` on a datagram channel returns a
byte channel reference to it. The byte view of the channel is simply the
concatenation of the contents of the datagrams.

This is only valid if, for the datagram channel:

- datagram boundaries have no significance or meaning,
- zero-length datagrams are not used to convey information, since they are
  meaningless for byte channels,
- short or zero-length writes through the byte API will not result in
  unacceptable overhead.

-------------
API reference
-------------
.. cpp:namespace:: pw::channel

.. doxygengroup:: pw_channel
   :content-only:
   :members:

Channel implementations
=======================
.. doxygengroup:: pw_channel_forwarding
   :content-only:
   :members:

.. doxygengroup:: pw_channel_loopback
   :content-only:
   :members:

.. doxygengroup:: pw_channel_epoll
   :content-only:
   :members:

.. doxygengroup:: pw_channel_rp2_stdio
   :content-only:
   :members:

.. doxygengroup:: pw_channel_stream_channel
   :content-only:
   :members:
