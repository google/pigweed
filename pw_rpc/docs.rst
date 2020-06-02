.. default-domain:: cpp

.. highlight:: sh

.. _chapter-pw-rpc:

------
pw_rpc
------
The ``pw_rpc`` module provides a system for defining and invoking remote
procedure calls (RPCs) on a device.

.. note::

  Under construction.

RPC server
==========
Declare an instance of ``rpc::Server`` and register services with it.

.. admonition:: TODO

  Document the public interface

RPC server implementation
-------------------------

The Method class
^^^^^^^^^^^^^^^^
The RPC Server depends on the ``pw::rpc::internal::Method`` class. ``Method``
serves as the bridge between the ``pw_rpc`` server library and the user-defined
RPC functions. ``Method`` takes an RPC packet, decodes it using a protobuf
library (if applicable), and calls the RPC function. Since ``Method`` interacts
directly with the protobuf library, it must be implemented separately for each
protobuf library.

``pw::rpc::internal::Method`` is not implemented as a facade with different
backends. Instead, there is a separate instance of the ``pw_rpc`` server library
for each ``Method`` implementation. There are a few reasons for this.

* ``Method`` is entirely internal to ``pw_rpc``. Users will never implement a
  custom backend. Exposing a facade would unnecessarily expose implementation
  details and make ``pw_rpc`` more difficult to use.
* There is no common interface between ``pw_rpc`` / ``Method`` implementations.
  It's not possible to swap between e.g. a Nanopb and a ``pw_protobuf`` RPC
  server because the interface for the user-implemented RPCs changes completely.
  This nullifies the primary benefit of facades.
* The different ``Method`` implementations can be built easily alongside one
  another in a cross-platform way. This makes testing simpler, since the tests
  build with any backend configuration. Users can select which ``Method``
  implementation to use simply by depending on the corresponding server library.

Packet flow
^^^^^^^^^^^

Requests
~~~~~~~~

.. blockdiag::

  blockdiag {
    packets [shape = beginpoint];

    group {
      label = "pw_rpc library"

      server [label = "Server"];
      service [label = "internal::Service"];
      method [label = "internal::Method"];
    }

    stubs [label = "generated services", shape = ellipse];
    user [label = "user-defined RPCs", shape = roundedbox];

    packets -> server -> service -> method -> stubs -> user;
    packets -> server [folded];
    method -> stubs [folded];
  }

Responses
~~~~~~~~~

.. blockdiag::

  blockdiag {
    user -> stubs [folded];

    group {
      label = "pw_rpc library"

      server [label = "Server"];
      method [label = "internal::Method"];
      channel [label = "Channel"];
    }

    stubs [label = "generated services", shape = ellipse];
    user [label = "user-defined RPCs", shape = roundedbox];
    packets [shape = beginpoint];

    user -> stubs -> method [folded];
    method -> server -> channel;
    channel -> packets [folded];
  }
