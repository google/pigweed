.. _module-pw_rpc-ts:

-------------------------
pw_rpc Typescript package
-------------------------
The ``pw_rpc`` Typescript package makes it possible to call Pigweed RPCs from
Typescript. The package includes client library to facilitate handling RPCs.

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
  unaryStub = channel.methodStub('pw.rpc.test1.TheTestService.SomeUnary')!
      as UnaryMethodStub;

The four possible RPC stubs are ``UnaryMethodStub``,
``ServerStreamingMethodStub``, ``ClientStreamingMethodStub``, and
``BidirectionalStreamingMethodStub``.  Note that ``channel.methodStub()``
returns a general stub. Since each stub type has different invoke
parameters, the general stub should be typecast before using.

Invoke an RPC with callbacks
============================

.. code-block:: typescript

  invoke(request?: Message,
      onNext: Callback = () => {},
      onCompleted: Callback = () => {},
      onError: Callback = () => {}): Call

All RPC methods can be invoked with a set of callbacks that are triggered when
either a response is received, the RPC is completed, or an error occurs. The
example below demonstrates registering these callbacks on a Bidirectional RPC.
Other RPC types can be invoked in a similar fashion. The request parameter may
differ slightly between RPC types.

.. code-block:: typescript

  bidiRpc = client.channel()?.methodStub(
      'pw.rpc.test1.TheTestService.SomeBidi')!
      as BidirectionalStreamingMethodStub;

  // Configure callback functions
  const onNext = (response: Message) => {
    console.log(response);
  }
  const onComplete = (status: Status) => {
    console.log('completed!');
  }
  const onError = (error: Error) => {
    console.log();
  }

  bidiRpc.invoke(request, onNext, onComplete, onError);

Open an RPC: ignore initial errors
=====================================

Open allows you to start and register an RPC without crashing on errors. This
is useful for starting an RPC before the server is ready. For instance, starting
a logging RPC while the device is booting.

.. code-block:: typescript

  open(request?: Message,
      onNext: Callback = () => {},
      onCompleted: Callback = () => {},
      onError: Callback = () => {}): Call

Blocking RPCs: promise API
==========================

Each MethodStub type provides an call() function that allows sending requests
and awaiting responses through the promise API. The timeout field is optional.
If no timeout is specified, the RPC will wait indefinitely.

Unary RPC
---------
.. code-block:: typescript

  unaryRpc = client.channel()?.methodStub(
      'pw.rpc.test1.TheTestService.SomeUnary')!
      as UnaryMethodStub;
  const request = new unaryRpc.requestType();
  request.setFooProperty(4);
  const timeout = 2000 // 2 seconds
  const [status, response] = await unaryRpc.call(request, timeout);

Server Streaming RPC
--------------------
.. code-block:: typescript

  serverStreamRpc = client.channel()?.methodStub(
      'pw.rpc.test1.TheTestService.SomeServerStreaming')!
      as ServerStreamingMethodStub;

  const call = serverStreamRpc.invoke();
  const timeout = 2000
  for await (const response of call.getResponses(2, timeout)) {
   console.log(response);
  }
  const responses = call.getResponse() // All responses until stream end.
  while (!responses.done) {
    console.log(await responses.value());
  }


Client Streaming RPC
--------------------
.. code-block:: typescript

  clientStreamRpc = client.channel()!.methodStub(
    'pw.rpc.test1.TheTestService.SomeClientStreaming')!
    as ClientStreamingMethodStub;
  clientStreamRpc.invoke();
  const request = new clientStreamRpc.method.requestType();
  request.setFooProperty('foo_test');
  clientStreamRpc.send(request);

  // Send three more requests, end the stream, and wait for a response.
  const timeout = 2000 // 2 seconds
  request.finishAndWait([request, request, request], timeout)
      .then(() => {
        console.log('Client stream finished successfully');
      })
      .catch((reason) => {
        console.log(`Client stream error: ${reason}`);
      });

Bidirectional Stream RPC
------------------------
.. code-block:: typescript

  bidiStreamingRpc = client.channel()!.methodStub(
    'pw.rpc.test1.TheTestService.SomeBidiStreaming')!
    as BidirectionalStreamingMethodStub;
  bidiStreamingRpc.invoke();
  const request = new bidiStreamingRpc.method.requestType();
  request.setFooProperty('foo_test');

  // Send requests
  bidiStreamingRpc.send(request);

  // Receive responses
  const timeout = 2000 // 2 seconds
  for await (const response of call.getResponses(1, timeout)) {
   console.log(response);
  }

  // Send three more requests, end the stream, and wait for a response.
  request.finishAndWait([request, request, request], timeout)
      .then(() => {
        console.log('Bidirectional stream finished successfully');
      })
      .catch((reason) => {
        console.log(`Bidirectional stream error: ${reason}`);
      });
