.. _module-pw_router:

---------
pw_router
---------
The ``pw_router`` module provides transport-agnostic classes for routing packets
over network links.

Common router interfaces
========================

PacketParser
------------
To work with arbitrary packet formats, routers require a common interface for
extracting relevant packet data, such as the destination. This interface is
``pw::router::PacketParser``, defined in ``pw_router/packet_parser.h``, which
must be implemented for the packet framing format used by the network.

Egress
------
The Egress class is a virtual interface for sending packet data over a network
link. Egress implementations provide a single ``SendPacket`` function, which
takes the raw packet data and transmits it.

Some common egress implementations are provided upstream in Pigweed.

StaticRouter
============
``pw::router::StaticRouter`` is a router with a static table of address to
egress mappings. Routes in a static router never change; packets with the same
address are always sent through the same egress. If links are unavailable,
packets will be dropped.

Static routers are suitable for basic networks with persistent links.

Usage example
-------------

.. code-block:: c++

  namespace {

  // Define packet parser and egresses.
  HdlcFrameParser hdlc_parser;
  UartEgress uart_egress;
  BluetoothEgress ble_egress;

  // Define the routing table.
  constexpr pw::router::StaticRouter::Route routes[] = {{1, uart_egress},
                                                        {7, ble_egress}};
  pw::router::StaticRouter router(hdlc_parser, routes);

  }  // namespace

  void ProcessPacket(pw::ConstByteSpan packet) {
    router.RoutePacket(packet);
  }

.. TODO(frolv): Re-enable this when the size report builds.
.. Size report
.. -----------
.. The following size report shows the cost of a ``StaticRouter`` with a simple
.. ``PacketParser`` implementation and a single route using an ``EgressFunction``.

.. .. include:: static_router_size
