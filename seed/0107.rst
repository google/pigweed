.. _seed-0107:

============================
0107: Pigweed Communications
============================
.. seed::
   :number: 107
   :name: Communications
   :status: Accepted
   :proposal_date: 2023-07-19
   :cl: 157090
   :authors: Wyatt Hepler
   :facilitator: Carlos Chinchilla

-------
Summary
-------
Pigweed does not currently offer an end-to-end solution for network
communications. This SEED proposes that Pigweed adopt a new sockets API as its
primary networking abstraction. The sockets API will be backed by a new,
lightweight embedded-focused network protocol stack, inspired by the Internet
protocol suite (TCP/IP). It will also support full TCP/IP via an open source
embedded TCP/IP stack or OS sockets. The new communications APIs will support
asynchronous use and zero-copy transmission.

This work is comprised of the following subareas:

- `Sockets API`_
- `Network protocol stack`_
- `Async`_ API pattern
- `Buffer management`_ system

The Pigweed team will revisit :ref:`pw_rpc <module-pw_rpc>` after deploying the
sockets API and network protocol stack.

----------
Background
----------
Pigweed's primary communications system is :ref:`pw_rpc <module-pw_rpc>`. pw_rpc
makes it possible to call functions and exchange data with a remote system.
Requests and responses are encoded as protobufs. pw_rpc was initially deployed
to a system with its own network protocol stack, so the Pigweed team did not
invest in building a network stack of its own.

The TCP/IP model, as described in `RFC 1122
<https://datatracker.ietf.org/doc/html/rfc1122>`_, organizes communications
systems and protocols into four layers: Application, Transport, Internet (or
Network), and Link (or Network access). Pigweed's current communications
offerings fit into the TCP/IP model as follows:

+-----------------------+-----------------------------+
| TCP/IP Model          | Pigweed Modules             |
+=======================+=============================+
| Application           | | :ref:`module-pw_transfer` |
|                       | | :ref:`module-pw_rpc`      |
+-----------------------+-----------------------------+
| Transport             | | :ref:`module-pw_router`   |
+-----------------------+ | :ref:`module-pw_hdlc`     |
| Internet / Network    |                             |
+-----------------------+                             |
| Link / Network access |                             |
+-----------------------+-----------------------------+

Notably, Pigweed provides little functionality below the application layer. The
pw_router and pw_hdlc modules only implement a subset of features needed at
their layer in the communications stack.

Challenges deploying pw_rpc
===========================
pw_rpc is application-layer communications module. It relies on a network layer
to send packets between endpoints and doesn't provide any networking features
itself. When initially developing pw_rpc, the Pigweed team focused its limited
resources solely on this application-layer feature, which made it possible to
deploy pw_rpc quickly to systems with existing networks.

pw_rpc has been deployed to many projects with great results. However, since
Pigweed does not provide a network stack, deploying pw_rpc to systems without
existing stacks can be challenging. These systems have to develop their own
solutions to transmit and route pw_rpc packets.

As an example, one project based its network communications on Pigweed's
:ref:`module-pw_hdlc` module. It used HDLC in a way more similar to IP,
providing network-level addressing and features like quality-of-service. Source
and destination addresses and ports were packed into the HDLC address field to
facilitate routing and multiplexing. The :ref:`module-pw_router` module was
developed to support static routing tables for HDLC frames through nodes in the
system, and the :ref:`pw_transfer RPC service <module-pw_transfer>` was
developed to provide reliable delivery of data.

Learning from custom network stacks
-----------------------------------
Teams want to use Pigweed to build cool devices. Their goal isn't to build a
network protocol stack, but they need one to use features like pw_rpc and
pw_transfer. Given this, teams have little incentive to make the enormous time
investment to develop a robust, reusable network stack. The practical approach
is to assemble the minimum viable network stack from what's available.

The Pigweed team has seen a few teams create custom network stacks for pw_rpc.
While these projects were successful, their network stacks were not their
primary focus. As a result, they had some shortcomings, including the following:

- **Byte stuffing memory overhead** -- HDLC is a low-level protocol. It uses
  `byte stuffing
  <https://en.wikipedia.org/wiki/High-Level_Data_Link_Control#Asynchronous_framing>`_
  to ensure frame integrity across unreliable links. Byte stuffing makes sense
  on the wire, but not in memory. Storing byte stuffed frames requires double
  the memory to account for worst-case byte stuffing. Some projects use HDLC
  frames as network layer packets, so they are buffered in memory for routing,
  which requires more memory than necessary.
- **HDLC protocol overhead** -- HDLC's frame recovery and integrity features are
  not needed across all links. For example, these features are unnecessary for
  Bluetooth. However, when projects use HDLC for both the network and link
  layers, it has to be used across all links.
- **pw_transfer at the application layer** -- :ref:`pw_transfer
  <module-pw_transfer>` supports reliable data transfers with :ref:`pw_rpc
  <module-pw_rpc>`. It required significant investment to develop, but since it
  is layered on top of pw_rpc, it has additional overhead and limited
  reusability.
- **Custom routing** -- Some network nodes have multiple routes between them.
  Projects have had to write custom, non-portable logic to handle routing.
- **pw_rpc channel IDs in routing** -- Some projects used pw_rpc channel IDs as
  a network addresses. Channel IDs were assigned for the whole network ahead of
  time. This has several downsides:

  - Requires nodes to have knowledge of the global channel ID assignments
    and routes between them, which can be difficult to keep in sync.
  - Implies that all traffic is pw_rpc packets.
  - Requires decoding pw_rpc packets at lower levels of the network stack.
  - Complicates runtime assignment of channel IDs.

- **Flow control** -- Projects' communications stacks have not supported flow
  control. The network layer simply has to drop packets it cannot process.
  There is no mechanism to tell the producer to slow down or wait for the
  receiver to be ready.
- **Accounting for the MTU** -- HDLC and pw_rpc have variable overheads, so it
  is difficult to know how much memory to allocate for RPC payloads. If packets
  are not sized properly with respect to the maximum transmission unit (MTU),
  packets may be silently dropped.

Problem summary
===============
These are the key issues of Pigweed's communications offerings based on the
team's experiences deploying pw_rpc.

**No cohesive full stack solution**

Pigweed only provides a handful of communications modules. They were not
designed to work together, and there is not enough to assemble a functioning
network stack. Some projects have to create bespoke network protocols with
limited reusability.

**Layering violations**

pw_transfer runs on top of pw_rpc instead of the transport layer, which adds
overhead and prevents its use independent of pw_rpc. Using pw_rpc channels for
routing ties the network to pw_rpc. Projects often use pw_hdlc for multiple
network layers, which brings the encoding's overhead higher up the stack and
across links that do not need it.

**Inefficiency**

Reliable data transfer requires pw_transfer, which runs on top of pw_rpc. This
adds additional overhead and requires more CPU-intensive decoding operations.
Using pw_rpc channel IDs in lower layers of the network requires expensive
varint decodes, even when the packets are bound for other nodes.

**Missing features**

Each project has to develop its own version of common features, including:

- **Addressing** -- There are no standard addressing schemes available to
  Pigweed users.
- **Routing** -- Projects must implement their own logic for routing packets,
  which can be complex.
- **Flow control** -- There is no way for the receiver to signal that it is ready
  for more data or that it cannot receive any more, either at the protocol or
  API level anywhere in the stack. Flow control is a crucial feature for
  realistic networks with limited resources.
- **Connections** -- Connections ensure the recipient is listening to
  transmissions, and detect when the other end is no longer communicating.
  pw_transfer maintains a connection, but it sits atop pw_rpc, so cannot be used
  elsewhere.
- **Quality of service (QoS)** -- Projects have developed basic QoS features in
  HDLC, but there is no support in upstream Pigweed. Every project has to
  develop its own custom implementation.

-----
Goals
-----
This SEED proposes a new communications system for Pigweed with the following
goals:

- **Practical end-to-end solution** -- Pigweed provides a full suite of APIs
  and protocols that support simple and complex networking use cases.
- **Robust, stable, and reliable** -- Pigweed communications "just work", even
  under high load. The networking stack is thoroughly tested in both single and
  multithreaded environments, with functional, load, fuzz, and performance
  testing. Projects can easily test their own deployments with Pigweed tooling.
- **Cohesive, yet modular** -- The network stack is holistically designed, but
  modular. It is organized into layers that can be exchanged and configured
  independently. Layering simplifies the stack, decouples protocol
  implementations, and maximizes flexibility within a cohesive system.
- **Efficient & performant** -- Pigweed’s network stack minimizes code size and
  CPU usage. It provides for high throughput, low latency data transmission.
  Memory allocation is configurable and adaptable to a project’s needs.
- **Usable & easy to learn** -- Pigweed’s communications systems are backed by
  thorough and up-to-date documentation. Getting started is easy using
  Pigweed's tutorials and examples.

--------
Proposal
--------
Pigweed will unify its communications systems under a common sockets API. This
entails the following:

- **Sockets API** -- Pigweed will introduce a `sockets
  API`_ to serve as its common networking interface.
- **Lightweight protocol stack** -- Pigweed will provide a custom,
  :ref:`lightweight network protocol stack <seed-0107-network-stack>` inspired
  by IPv6, with UDP, TCP, and SCTP-like transport protocols.
- **TCP/IP integration** -- Pigweed will offer sockets implementations for OS
  sockets and an existing `embedded TCP/IP stack`_.
- **Async** -- Pigweed will establish a new pattern for `async`_ programming and
  use it in its networking APIs.
- **Zero copy** -- Pigweed will develop a new `buffer management`_ system to
  enable zero-copy networking.

These features fit fit into the TCP/IP model as follows:

+-------------------------------------+-------------------------------------+
| TCP/IP Model                        | Future Pigweed Comms Stack          |
+=====================================+=====================================+
| Application                         | | *Various modules including*       |
|                                     | | *pw_rpc and pw_transfer.*         |
|                                     |                                     |
|                                     |                                     |
|                                     |                                     |
+-------------------------------------+-------------------------------------+
| .. rst-class:: pw-text-center-align | .. rst-class:: pw-text-center-align |
|                                     |                                     |
|    **OS Sockets**                   |    **Pigweed Sockets**              |
+-------------------------------------+-------------------------------------+
| Transport                           | | UDP-like unreliable protocol      |
|                                     | | TCP-like reliable protocol        |
|                                     | | SCTP-like reliable protocol       |
+-------------------------------------+-------------------------------------+
| Network / Internet                  | | IPv6-like protocol                |
+-------------------------------------+-------------------------------------+
| Network access / Link               | | HDLC                              |
|                                     | | others                            |
+-------------------------------------+-------------------------------------+

Sockets API
===========
The new sockets API will become the primary networking abstraction in Pigweed.
The API will support the following:

- Creating sockets for bidirectional communications with other nodes in the
  network.
- Opening and closing connections for connection-oriented socket types.
- Sending and receiving data, optionally :ref:`asynchronously
  <seed-0107-async>`.
- Reporting errors.

The sockets API will support runtime polymorphism. In C++, it will be a virtual
interface.

**Rationale**

A network socket represents a bidirectional communications channel with another
node, which could be local or across the Internet. Network sockets form the API
between an application and the network.

Sockets are a proven, well-understood concept. Socket APIs such as Berkeley /
POSIX sockets are familiar to anyone with Internet programming experience.

Sockets APIs hide the details of the network protocol stack. A socket provides
well-defined semantics for a communications channel, but applications do not
need to know how data is sent and received. The same API can be used to exchange
data with another process on the same machine or with a device across the world.

.. admonition:: Sockets SEEDs

   The Pigweed sockets API is described in SEED-0116. The sockets API is based
   on ``pw_channel``, which is proposed in SEED-0114.

Socket types
------------
Pigweed's sockets API will support the following sockets types.

.. list-table::
   :header-rows: 1

   * - Berkeley socket type
     - Internet protocol
     - Payload type
     - Connection-oriented
     - Guaranteed, ordered delivery
     - Description
   * - ``SOCK_DGRAM``
     - UDP
     - Datagram
     - ❌
     - ❌
     - Unreliable datagram
   * - ``SOCK_STREAM``
     - TCP
     - Byte stream
     - ✅
     - ✅
     - Reliable byte stream
   * - ``SOCK_SEQPACKET``
     - SCTP
     - Datagram
     - ✅
     - ✅
     - Reliable datagram

Raw sockets (``SOCK_RAW``) may be supported in the future if required.
``SOCK_CONN_DGRAM`` (unreliable connection-oriented datagram) sockets are
uncommon and will not be supported.

The socket's semantics will be expressed in the sockets API, e.g. with a
different interface or class for each type. Instances of the connection-oriented
socket types will be generated from a "listener" object.

Pigweed's sockets API will draw inspiration from modern type safe APIs like
Rust's `std::net sockets <https://doc.rust-lang.org/std/net/index.html>`_,
rather than traditional APIs like POSIX sockets or Winsock. Pigweed sockets will
map trivially to these APIs and implementations will be provided upstream.

Using the sockets API
---------------------
The Pigweed sockets API will provide the interface between applications and the
network. Any application can open a socket to communicate across the network.
A future revision of ``pw_rpc`` will use the sockets API in place of its current
``Channel`` API.

The sockets API will support both synchronous and :ref:`asynchronous
<seed-0107-async>` use. The synchronous API may be built using the async API.
It will also support :ref:`zero-copy <seed-0107-buffers>` data transmission.

Addressing
----------
The Pigweed sockets API will be aware of addresses. Addresses are used to refer
to nodes in a network, including the socket's own node. With TCP/IP, the socket
address includes an IP address and a port number.

The POSIX sockets API supports different domains through address family
constants such as ``AF_INET``, ``AF_INET6``, and ``AF_UNIX``. Addresses in these
families are specified or accessed in various socket operations. Because the
address format is not specified by the API, working with addresses is not type
safe.

Pigweed sockets will approach addressing differently, but details are yet to be
determined. Possible approaches include:

- Use IPv6 addresses exclusively. Systems with other addressing schemes map
  these into IPv6 for use with Pigweed APIs.
- Provide a polymorphic address class so sockets can work with addresses
  generically.
- Avoid addresses in the base sockets API. Instead, use implementation specific
  derived classes to access addresses.

Network protocol stack
======================
The sockets API will be backed by a network protocol stack. Pigweed will provide
sockets implementations for following network protocol stacks:

* Third party embedded TCP/IP stack, most likely `lwIP
  <https://savannah.nongnu.org/projects/lwip/>`_.
* Operating system TCP/IP stack via POSIX sockets or `Winsock
  <https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-start-page-2>`_.
* Custom :ref:`lightweight network protocol stack <seed-0107-network-stack>`.

Embedded TCP/IP stack
---------------------
Pigweed will provide a sockets implementation for an embedded TCP/IP stack such
as `lwIP <https://savannah.nongnu.org/projects/lwip/>`_.

The sockets integration will be structured to avoid unnecessary dependencies on
network stack features. For example, if a system is using IPv6 exclusively, the
integration won't require IPv4 support, and the TCP/IP stack can be configured
without it.

**Rationale**

The Internet protocol suite, or TCP/IP, is informed by decades of research and
practical experience. It is much more than IP, TCP, and UDP; it's an alphabet
soup of protocols that address a myriad of use cases and challenges.
Implementing a functional TCP/IP stack is no small task. At time of writing,
lwIP has about as many lines of C as Pigweed has C++ (excluding tests).

The Pigweed team does not plan to implement a full TCP/IP stack. This is a major
undertaking, and there are already established open source embedded TCP/IP
stacks. Projects needing the full power of TCP/IP can use an embedded stack like
`lwIP <https://savannah.nongnu.org/projects/lwip/>`_.

Choosing between embedded TCP/IP and :ref:`Pigweed's stack <seed-0107-network-stack>`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
lwIP's `website <https://savannah.nongnu.org/projects/lwip/>`_ states that it
requires tens of KB of RAM and about 40 KB of ROM. Using lwIP means using the
same TCP/IP protocols that run the Internet. These protocols are feature rich,
but have more overhead than is necessary for local communications within a small
embedded system.

Projects that can afford the resource requirements and protocol overhead of
TCP/IP should use it. These projects can set up a local IPv4 or IPv6 network
and use it for communications behind the Pigweed sockets API. Projects that
cannot afford full TCP/IP can opt for Pigweed's :ref:`custom protocol stack
<seed-0107-network-stack>`. Pigweed's custom stack will not have the depth of
features and tooling of TCP/IP does, but will be sufficient for many systems.

TCP/IP socket types
^^^^^^^^^^^^^^^^^^^
With an embedded TCP/IP stack, the Pigweed sockets API will be implemented as
follows:

- Unreliable datagram (``SOCK_DGRAM``) -- UDP
- Reliable byte stream (``SOCK_STREAM``) -- TCP
- Reliable datagram (``SOCK_SEQPACKET``) -- Lightweight framing over TCP. This
  will be semantically similar to `SCTP
  <https://datatracker.ietf.org/doc/html/rfc9260>`_, but integrations will not
  use SCTP since it is not widely supported.

.. _seed-0107-network-stack:

Pigweed's custom network protocol stack
---------------------------------------
Pigweed will develop a custom, lightweight network protocol stack.

This new protocol stack will be designed for small devices with relatively
simple networks. It will scale to several interconnected cores that interface
with a few external devices (e.g. over USB or Bluetooth). Depending on project
requirements, it may or may not support dynamic network host configuration (e.g.
DHCP or SLAAC).

Pigweed's network protocol stack will be a strict subset of TCP/IP. This will
include minimal, reduced overhead versions of UDP, TCP, and IPv6. Portions of
other protocols such as ICMPv6 may be implemented as required.

**Rationale**

TCP/IP is too large and complex for some embedded systems. Systems for which
TCP/IP is unnecessary can use Pigweed's lightweight embedded network protocol
stack.

Transport layer
^^^^^^^^^^^^^^^
Pigweed will provide transport layer protocols that implement the semantics of
``SOCK_DGRAM``, ``SOCK_STREAM``, and ``SOCK_SEQPACKET``-like sockets.

- ``SOCK_DRAM``-like sockets will be backed by a UDP-like protocol. This will
  add source and destination ports to the IP-style packets for multiplexing on
  top of the network layer.
- ``SOCK_STREAM``-like sockets will be backed by a TCP-like protocol that uses
  network layer packets to implement a reliable byte stream. It will be based on
  TCP, but will not implement all of its features. The :ref:`module-pw_transfer`
  module may serve as a starting point for the new protocol implementation.
- ``SOCK_SEQPACKET``-like sockets will be implemented with a simple
  message-oriented protocol on top of the TCP-like protocol. Applications like
  pw_rpc will use ``SOCK_SEQPACKET`` sockets.

Network layer
^^^^^^^^^^^^^
Pigweed will create a new network-layer protocol closely based on IPv6. Details
are still to be determined, but the protocol is intended to be a strict subset
of IPv6 and related protocols (e.g. ICMP, NDP) as needed. If a need arises, it
is met by implementing the associated IP suite protocol. Packets will use
compressed version of an IPv6 header (e.g. omit fields, use smaller addresses).

This protocol will provide:

- Unreliable packet delivery between source and destination.
- Routing based on the source and destination addresses.
- Quality of service (e.g. via the traffic class field).

Packets may be routed at this layer independently of the link layer. Wire format
details stay on the wire.

Network access / link layer
^^^^^^^^^^^^^^^^^^^^^^^^^^^
Pigweed's network stack will interact with the link layer through a generic
interface. This will allow Pigweed to send network packets with any protocol
over any physical interface.

Pigweed already provides minimal support for one link layer protocol, HDLC.
Other protocols (e.g. COBS, PPP) may be implemented. Some hardware interfaces
(e.g. Bluetooth, USB) may not require an additional link-layer protocol.

Language support
----------------
Pigweed today is primarily C++, but it supports Rust, C, Python, TypeScript, and
Java to varying extents.

Pigweed’s communications stack will be developed in either C++ or Rust to start,
but it will be ported to all supported languages in time. The stack may have C
APIs to facilitate interoperability between C++ and Rust.

.. admonition:: Network protocol stack SEED

   Pigweed's network protocol stack will be explored in an upcoming SEED.

.. _seed-0107-async:

Async
=====
Pigweed will develop a model for asynchronous programming and use it in its
networking APIs, including sockets. Sockets will also support synchronous
operations, but these may be implemented in terms of the asynchronous API.

The Pigweed async model has not been designed yet. The :ref:`pw_async
<module-pw_async>` module has a task dispatcher, but the pattern for async APIs
has not been established. Further exploration is needed, but C++20 coroutines
may be used for Pigweed async APIs where supported.

**Rationale**

Synchronous APIs require the thread to block while an operation completes. The
thread and its resources cannot be used by the system until the task completes.
Async APIs allow a single thread to handle multiple simultaneous tasks. The
thread advances tasks until they need to wait for an external operation to
complete, then switches to another task to avoid blocking.

Threads are expensive in embedded systems. Each thread requires significant
memory for its stack and kernel structures for bookkeeping. They occupy this
memory all the time, even when they are not running. Furthermore, context
switches between threads can take significant CPU time.

Asynchronous programming avoids these downsides. Many asynchronous threads run
on a single thread. Fewer threads are needed, and the resources of one thread
are shared by multiple tasks. Since asynchronous systems run within one thread,
no thread context switches occur.

Networking involves many asynchronous tasks. For example, waiting for data to be
sent through a network interface, for a connection request, or for data to
arrive on one or more interfaces are all operations that benefit from
asynchronous APIs. Network protocols themselves are heavily asynchronous.

.. admonition:: Async SEED

   Pigweed's async pattern is proposed in :ref:`SEED-0112 <seed-0112>`.

.. _seed-0107-buffers:

Buffer management
=================
Pigweed's networking APIs will support zero-copy data transmission. Applications
will be able to request a buffer from a socket. When one is available, they fill
it with data for transmission.

Pigweed will develop a general purpose module for allocating and managing
buffers. This will be used to implement zero-copy features for Pigweed's
networking stack.

As an example, zero-copy buffer allocation could work as follows:

- The user requests a buffer from a socket.
- The network protocol layer under the socket requests a buffer from the next
  lower layer.
- The bottom protocol layer allocates a buffer.
- Each layer reserves part of the buffer for its headers or footers.
- The remaining buffer is provided to the user to populate with their payload.
- When the user is done, the buffer is released. Each layer of the network stack
  processes the buffer as necessary.
- Finally, at the lowest layer, the final buffer is sent over the hardware
  interface.

Zero-copy APIs will be :ref:`asynchronous <seed-0107-async>`.

**Rationale**

Networking involves transmitting large amounts of data. Copying network traffic
can result in substantial CPU usage, particularly in nodes that route traffic to
other nodes.

A buffer management system that minimizes copying saves precious CPU cycles and
power on constrained systems.

.. admonition:: Buffer management SEED

   Pigweed's buffer management system is proposed in :ref:`SEED-0109
   <seed-0109>`.

Vectored I/O
------------
Vectored or scatter/gather I/O allows users to serialize data from multiple
buffers into a single output stream, or vice versa. For Pigweed's networking
APIs, this could be used to, for example, store a packet header in one buffer
and packet contents in one or more other buffers. These isolated chunks are
serialized in order to the network interface.

Vectored I/O minimizes copying, but is complex. Additionally, simple DMA engines
may only operate on a single DMA buffer. Thus, vectored I/O could require
either:

- a copy into the DMA engine's buffer, which costs CPU time and memory, or
- multiple, small DMAs, which involves extra interrupts and CPU time.

Vectored I/O may be supported in Pigweed's communications stack, depending on
project requirements.

----------
Next steps
----------
Pigweed's communications revamp will proceed loosely as follows:

* Write SEEDs to explore existing solutions, distill requirements, and propose
  new Pigweed features for these areas:

  - Sockets API (SEED-0116)
  - Async pattern (:ref:`SEED-0112 <seed-0112>`).
  - Buffer management (:ref:`SEED-0109 <seed-0109>`)
  - Network protocol stack

* Implement the Sockets API.

  - Document, integrate, and deploy the async programming pattern for Pigweed.
  - Develop and test Pigweed's buffer management system.
  - Use these features in the sockets API. If necessary, the synchronous,
    copying API could be implemented first.

* Deploy the sockets API for TCP/IP.

  - Implement and unit test sockets for TCP/IP with POSIX and Winsock sockets.
  - Implement and unit test sockets for an embedded TCP/IP stack.

* Develop a test suite for Pigweed network communications.

  - Create integration tests for networks with multiple nodes that cover basic
    operation, high load, and packet loss.
  - Write performance tests against the sockets API to measure network stack
    performance.

* Develop Pigweed's lightweight network protocol stack.

  - Test the lightweight network protocol stack on hardware and in a simulated
    environment.
  - Write fuzz tests for the protocol stack.
  - Write performance tests for the protocol stack.

* Revisit other communications systems, including pw_rpc and pw_transfer.
