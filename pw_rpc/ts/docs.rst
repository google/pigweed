.. _module-pw_rpc-ts:

-------------------------
pw_rpc Typescript package
-------------------------
The ``pw_rpc`` Typescript package makes it possible to call Pigweed RPCs from
Typescript. The package includes a ``pw_rpc`` client library to facilitate
handling RPCs.

This package is currently a work in progress.

Creating an RPC Client
======================
The RPC client is instantiated from a list of channels and a set of protos.

.. code-block:: typescript

  const testProtoPath = 'pw_rpc/ts/test_protos-descriptor-set.proto.bin';
  const lib = await Library.fromFileDescriptorSet(
    testProtoPath, 'test_protos_tspb');
  const channels = [new Channel(1, savePacket), new Channel(5)];
  const client = Client.fromProtoSet(channels, lib);

  function savePacket(packetBytes: Uint8Array): void {
    const packet = RpcPacket.deserializeBinary(packetBytes);
    ...
  }

The proto library must match the proto build rules. The first argument
corresponds with the location of the ``proto_library`` build rule that generates
a descriptor set for all src protos. The second argument corresponds with the
name of the ``js_proto_library`` build rule that generates javascript based on
the descriptor set. For instance, the previous example corresponds with the
following build file: ``pw_rpc/ts/BUILD.bazel``.

.. code-block::

  proto_library(
      name = "test_protos",
      srcs = [
          "test.proto",
          "test2.proto",
      ],
  )

  js_proto_library(
      name = "test_protos_tspb",
      protos = [":test_protos"],
  )

Finding an RPC Method
=====================
Once the client is instantiated with the correct proto library, the target RPC
method is found by searching based on the full name:
``{packageName}.{serviceName}.{methodName}``

.. code-block:: typescript

  const channel = client.channel()!;
  const stub = channel.methodStub('pw.rpc.test1.TheTestService.SomeUnary')!;

Calling an RPC
==============

Unary RPC
---------
Only non blocking calls are currently supported.

.. code-block:: typescript

  unaryStub = client.channel()?.methodStub(
      'pw.rpc.test1.TheTestService.SomeUnary')!;
  request = new unaryStub.method.requestType();
  request.setFooProperty('hello world');
  const call = unaryStub.invoke(request, (response) => {
    console.log(response);
  });

Server Streaming RPC
--------------------
Unsupported

Client Streaming RPC
--------------------
Unsupported

Bidirectional Stream RPC
------------------------
Unsupported

.. attention::

  RPC timeout is currently unsupported on all RPC types.

