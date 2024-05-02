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

--------------------
Creating a transport
--------------------
RPC transports implement ``pw::rpc::RpcFrameSender``. The transport exposes its
maximum transmission unit (MTU) and only knows how to send packets of up to the
size of that MTU.

.. code-block:: cpp

   class MyRpcTransport : public RpcFrameSender {
   public:
     size_t mtu() const override { return 128; }

     Status Send(RpcFrame frame) override {
       // Send the frame via mailbox, shared memory or some other mechanism...
     }
   };

--------------------------
Integration with pw_stream
--------------------------
An RpcFrameSender implementaion that wraps a ``pw::stream::Writer`` is provided
by ``pw::rpc::StreamRpcFrameSender``. As the stream interface doesn't know
about MTU's, it's up to the user to select one.

.. code-block:: cpp

   stream::SysIoWriter writer;
   StreamRpcFrameSender<kMtu> sender(writer);

A thread to feed data to a ``pw::rpc::RpcIngressHandler`` from a
``pw::stream::Reader`` is provided by ``pw::rpc::StreamRpcDispatcher``.

.. code-block:: cpp

   rpc::HdlcRpcIngress<kMaxRpcPacketSize> hdlc_ingress(...);
   stream::SysIoReader reader;

   // Feed Hdlc ingress with bytes from sysio.
   rpc::StreamRpcDispatcher<kMaxSysioRead> sysio_dispatcher(reader,
                                                            hdlc_ingress);

   thread::DetachedThread(SysioDispatcherThreadOptions(),
                          sysio_dispatcher);

-------------------------------------------
Using transports: a sample three-node setup
-------------------------------------------

A transport must be properly registered in order for ``pw_rpc`` to correctly
route its packets. Below is an example of using a ``SocketRpcTransport`` and
a (hypothetical) ``SharedMemoryRpcTransport`` to set up RPC connectivity between
three endpoints.

Node A runs ``pw_rpc`` clients who want to talk to nodes B and C using
``kChannelAB`` and ``kChannelAC`` respectively. However there is no direct
connectivity from A to C: only B can talk to C over shared memory while A can
talk to B over a socket connection. Also, some services on A are self-hosted
and accessed from the same process on ``kChannelAA``:

.. code-block:: cpp

   // Set up A->B transport over a network socket where B is a server
   // and A is a client.
   SocketRpcTransport<kSocketReadBufferSize> a_to_b_transport(
     SocketRpcTransport<kSocketReadBufferSize>::kAsClient, "localhost",
     kNodeBPortNumber);

   // LocalRpcEgress handles RPC packets received from other nodes and destined
   // to this node.
   LocalRpcEgress<kLocalEgressQueueSize, kMaxPacketSize> local_egress;
   // HdlcRpcEgress applies HDLC framing to all packets outgoing over the A->B
   // transport.
   HdlcRpcEgress<kMaxPacketSize> a_to_b_egress("a->b", a_to_b_transport);

   // List of channels for all packets originated locally at A.
   std::array tx_channels = {
     // Self-destined packets go directly to local egress.
     Channel::Create<kChannelAA>(&local_egress),
     // Packets to B and C go over A->B transport.
     Channel::Create<kChannelAB>(&a_to_b_egress),
     Channel::Create<kChannelAC>(&a_to_b_egress),
   };

   // Here we list all egresses for the packets _incoming_ from B.
   std::array b_rx_channels = {
     // Packets on both AB and AC channels are destined locally; hence sending
     // to the local egress.
     ChannelEgress{kChannelAB, local_egress},
     ChannelEgress{kChannelAC, local_egress},
   };

   // HdlcRpcIngress complements HdlcRpcEgress: all packets received on
   // `b_rx_channels` are assumed to have HDLC framing.
   HdlcRpcIngress<kMaxPacketSize> b_ingress(b_rx_channels);

   // Local egress needs to know how to send received packets to their target
   // pw_rpc service.
   ServiceRegistry registry(tx_channels);
   local_egress.set_packet_processor(registry);
   // Socket transport needs to be aware of what ingress it's handling.
   a_to_b_transport.set_ingress(b_ingress);

   // Both RpcSocketTransport and LocalRpcEgress are ThreadCore's and
   // need to be started in order for packet processing to start.
   DetachedThread(/*...*/, a_to_b_transport);
   DetachedThread(/*...*/, local_egress);

Node B setup is the most complicated since it needs to deal with egress
and ingress from both A and B and needs to support two kinds of transports. Note
that A is unaware of which transport and framing B is using when talking to C:

.. code-block:: cpp

   // This is the server counterpart to A's client socket.
   SocketRpcTransport<kSocketReadBufferSize> b_to_a_transport(
     SocketRpcTransport<kSocketReadBufferSize>::kAsServer, "localhost",
     kNodeBPortNumber);

   SharedMemoryRpcTransport b_to_c_transport(/*...*/);

   LocalRpcEgress<kLocalEgressQueueSize, kMaxPacketSize> local_egress;
   HdlcRpcEgress<kMaxPacketSize> b_to_a_egress("b->a", b_to_a_transport);
   // SimpleRpcEgress applies a very simple length-prefixed framing to B->C
   // traffic (because HDLC adds unnecessary overhead over shared memory).
   SimpleRpcEgress<kMaxPacketSize> b_to_c_egress("b->c", b_to_c_transport);

   // List of channels for all packets originated locally at B (note that in
   // this example B doesn't need to talk to C directly; it only proxies for A).
   std::array tx_channels = {
     Channel::Create<kChannelAB>(&b_to_a_egress),
   };

   // Here we list all egresses for the packets _incoming_ from A.
   std::array a_rx_channels = {
     ChannelEgress{kChannelAB, local_egress},
     ChannelEgress{kChannelAC, b_to_c_egress},
   };

   // Here we list all egresses for the packets _incoming_ from C.
   std::array c_rx_channels = {
     ChannelEgress{kChannelAC, b_to_a_egress},
   };

   HdlcRpcIngress<kMaxPacketSize> b_ingress(b_rx_channels);
   SimpleRpcIngress<kMaxPacketSize> c_ingress(c_rx_channels);

   ServiceRegistry registry(tx_channels);
   local_egress.set_packet_processor(registry);

   b_to_a_transport.set_ingress(a_ingress);
   b_to_c_transport.set_ingress(c_ingress);

   DetachedThread({}, b_to_a_transport);
   DetachedThread({}, b_to_c_transport);
   DetachedThread({}, local_egress);

Node C setup is straightforward since it only needs to handle ingress from B:

.. code-block:: cpp

   SharedMemoryRpcTransport c_to_b_transport(/*...*/);
   LocalRpcEgress<kLocalEgressQueueSize, kMaxPacketSize> local_egress;
   SimpleRpcEgress<kMaxPacketSize> c_to_b_egress("c->b", c_to_b_transport);

   std::array tx_channels = {
     Channel::Create<kChannelAC>(&c_to_b_egress),
   };

   // Here we list all egresses for the packets _incoming_ from B.
   std::array b_rx_channels = {
     ChannelEgress{kChannelAC, local_egress},
   };

   SimpleRpcIngress<kMaxPacketSize> b_ingress(b_rx_channels);

   ServiceRegistry registry(tx_channels);
   local_egress.set_packet_processor(registry);

   c_to_b_transport.set_ingress(b_ingress);

   DetachedThread(/*...*/, c_to_b_transport);
   DetachedThread(/*...*/, local_egress);
