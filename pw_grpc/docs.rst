.. _module-pw_grpc:

=======
pw_grpc
=======
.. pigweed-module::
   :name: pw_grpc

``pw_grpc`` is an implementation of the gRPC over HTTP2 protocol that utilizes
``pw_rpc`` for code generation and service hosting. It provides classes that map
between pw_rpc packets and gRPC HTTP2 frames, allowing pw_rpc services to be
exposed as gRPC services.

--------
Overview
--------
The ``Connection`` class implements the gRPC HTTP2 protocol on top of a socket
like stream. Create a new instance every time a new connection is established.
It will notify when new RPC calls are started, data is received, the call is
cancelled, and when the connection stream should be closed.

The ``PwRpcHandler`` class is what maps gRPC events provided by ``Connection``
instances to ``pw_rpc`` packets. It takes a ``pw::rpc::RpcPacketProcessor``
to forward packets to.

The ``GrpcChannelOutput`` class is what handles mapping outgoing ``pw_rpc``
packets back to the ``Connection`` send methods, which will translate to gRPC
responses.

Refer to the ``test_pw_rpc_server.cc`` file for detailed usage example of how to
integrate into a ``pw_rpc`` network.
