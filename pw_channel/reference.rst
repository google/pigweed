.. _module-pw_channel-reference:

=========
Reference
=========

.. _module-pw_channel-reference-cpp:

-----------------
C++ API reference
-----------------

.. pigweed-module-subpage::
   :name: pw_channel

.. cpp:namespace:: pw::channel

.. doxygengroup:: pw_channel
   :content-only:
   :members:

.. _module-pw_channel-reference-cpp-impl:

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

Packets
=======
.. doxygengroup:: pw_channel_packets
   :content-only:
   :members:

.. doxygenclass:: pw::channel::TestPacketReaderWriter
   :members:
