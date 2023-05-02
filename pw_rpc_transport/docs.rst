.. _module-pw_rpc_transport:

.. warning::
  This is an experimental module currently under development. APIs and
  functionality may change at any time.

================
pw_rpc_transport
================
The ``pw_rpc_transport`` provides a transport layer for ``pw_rpc``.

``pw_rpc`` provides a system for defining and invoking remote procedure calls
(RPCs) on a device. It does not include any transports for sending these RPC
calls. On a real device there could be multiple ways of inter-process and/or
inter-core communication: hardware mailboxes, shared memory, network sockets,
Unix domain sockets. ``pw_rpc_transport`` provides means to implement various
transports and integrate them with ``pw_rpc`` services.

``pw_rpc_transport`` relies on the assumption that a ``pw_rpc`` channel ID
uniquely identifies both sides of an RPC conversation. It allows developers to
define transports, egresses and ingresses for various channel IDs and choose
what framing will be used to send RPC packets over those transports.

RpcFrame
--------
Framed RPC data ready to be sent via ``RpcFrameSender``. Consists of a header
and a payload. Some RPC transport encodings may not require a header and put
all of the framed data into the payload (in which case the header can be
an empty span).

A single RPC packet can be split into multiple ``RpcFrame``'s depending on the
MTU of the transport.

All frames for an RPC packet are expected to be sent and received in order
without being interleaved by other packets' frames.

RpcFrameSender
--------------
Sends RPC frames over some communication channel (e.g. a hardware mailbox,
shared memory, or a socket). It exposes its MTU size and generally only knows
how to send an ``RpcFrame`` of a size that doesn't exceed that MTU.

RpcPacketEncoder / RpcPacketDecoder
-----------------------------------
``RpcPacketEncoder`` is used to split and frame an RPC packet.
``RpcPacketDecoder`` then does the opposite e.g. stitches together received
frames and removes any framing added by the encoder.

RpcEgressHandler
----------------
Provides means of sending an RPC packet to its destination. Typically it ties
together an ``RpcPacketEncoder`` and ``RpcFrameSender``.

RpcIngressHandler
-----------------
Provides means of receiving RPC packets over some transport. Typically it has
logic for reading RPC frames from some transport (a network connection,
shared memory, or a hardware mailbox), stitching and decoding them with
``RpcPacketDecoder`` and passing full RPC packets to their intended processor
via ``RpcPacketProcessor``.

RpcPacketProcessor
------------------
Used by ``RpcIngressHandler`` to send the received RPC packet to its intended
handler (e.g. a pw_rpc ``Service``).

