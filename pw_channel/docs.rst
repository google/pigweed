.. _module-pw_channel:

==========
pw_channel
==========
.. warning::

  This module is in an early, experimental state. Do not rely on its APIs.

.. cpp:namespace:: pw::channel

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
.. doxygengroup:: pw_channel
   :content-only:
   :members:

Channel implementations
=======================
.. doxygengroup:: pw_channel_forwarding
   :content-only:
   :members:
