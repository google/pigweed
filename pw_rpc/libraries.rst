.. _module-pw_rpc-libraries:

======================================
Client, server, and protobuf libraries
======================================
.. pigweed-module-subpage::
   :name: pw_rpc

.. grid:: 1

   .. grid-item-card:: :octicon:`code-square` C++ server and client
      :link: module-pw_rpc-cpp
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      C++ server and client library API guides. Server, client, and
      server/client libraries are available. Sharing code between
      servers and clients is possible. RPC calls are represented through
      a call class; both servers and clients use the same base call class.
      RPCs can be invoked asynchronously through callbacks or synchronously
      through a blocking API. RPC calls can be introspected to obtain
      information that was generated during compilation.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Python client
      :link: module-pw_rpc-py
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Python client library API reference. The Python client can
      send requests and handle responses for a set of channels.
      The callback-based API supports invoking RPCs asynchronously.
      There's also an utilities API for extending ``pw_console`` to
      interact with RPCs.

   .. grid-item-card:: :octicon:`code-square` TypeScript client
      :link: module-pw_rpc-ts
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      TypeScript client library API guide. Unary-streaming, server-streaming,
      client-streaming, and bi-directional streaming are supported.
      RPCs can be invoked asynchronously through callbacks or
      synchronously through promises.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Nanopb codegen
      :link: module-pw_rpc_nanopb
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Nanopb codegen library API guide. Generates services which
      encode and decode RPC requests and responses as Nanopb message
      structs.

   .. grid-item-card:: :octicon:`code-square` pw_protobuf codegen
      :link: module-pw_rpc_pw_protobuf
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      ``pw_protobuf`` codegen library API guide. Generates services which
      encode and decode RPC requests and responses as ``pw_protobuf``
      message structs.

.. toctree::
   :maxdepth: 1
   :hidden:

   C++ server and client <cpp>
   Python client <py/docs>
   TypeScript client <ts/docs>
   Nanopb codegen <nanopb/docs>
   pw_protobuf codegen <pwpb/docs>
