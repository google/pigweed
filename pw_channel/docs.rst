.. _module-pw_channel:

==========
pw_channel
==========
.. warning::

  This module is in an early, experimental state. Do not rely on its APIs.

----------------------------------------
Using datagram channels as byte channels
----------------------------------------
Datagram-oriented channels may be passed to APIs that take byte-oriented
channels. The byte view of the channel is simply the concatenation of the
contents of the datagrams.

It is important to note that zero-length reads and writes are are semantically
different between datagram-oriented and byte-oriented channels. With datagrams,
a zero-length write always produces a zero-length read on the other end. A
zero-length datagram may convey meaning.

For byte-oriented channels, zero-length reads and writes never convey meaning,
but are permitted so that datagram channels can be used as byte channels. A
zero-length write may or may not result in a zero-length read on the other end.
Byte channel implementations must ignore zero-length byte writes. Code reading
from a byte channel must ignore zero-length reads. The existence of a
zero-length byte read or write does not signal any information.

-------------
API reference
-------------
.. cpp:namespace:: pw::channel

.. doxygengroup:: pw_channel
   :content-only:
   :members:

Channel aliases
===============
.. doxygengroup:: pw_channel_aliases
   :content-only:
   :members:
