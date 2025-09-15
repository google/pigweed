.. _module-pw_rpc-guides:

===================
Quickstart & guides
===================
.. pigweed-module-subpage::
   :name: pw_rpc

.. _module-pw_rpc-quickstart:

----------
Quickstart
----------
This section guides you through the process of adding ``pw_rpc`` to your project.

Check out :ref:`module-pw_rpc-design-overview` for a conceptual explanation
of the RPC lifecycle.

1. RPC service declaration
==========================
Declare a :ref:`service <module-pw_rpc-design-services>` in a service
definition:

.. code-block:: protobuf

   /* //applications/blinky/blinky.proto */

   syntax = "proto3";

   package blinky;

   import "pw_protobuf_protos/common.proto";

   service Blinky {
     // Toggles the LED on or off.
     rpc ToggleLed(pw.protobuf.Empty) returns (pw.protobuf.Empty);
     // Continuously blinks the board LED a specified number of times.
     rpc Blink(BlinkRequest) returns (pw.protobuf.Empty);
   }

   message BlinkRequest {
     // The interval at which to blink the LED, in milliseconds.
     uint32 interval_ms = 1;
     // The number of times to blink the LED.
     optional uint32 blink_count = 2;
   } // BlinkRequest

Declare the protocol buffer in ``BUILD.bazel``:

.. code-block:: python

   # //applications/blinky/BUILD.bazel

   load(
       "@pigweed//pw_protobuf_compiler:pw_proto_library.bzl",
       "nanopb_proto_library",
       "nanopb_rpc_proto_library",
   )

   proto_library(
       name = "proto",
       srcs = ["blinky.proto"],
       deps = [
           "@pigweed//pw_protobuf:common_proto",
       ],
   )

   nanopb_proto_library(
       name = "nanopb",
       deps = [":proto"],
   )

   nanopb_rpc_proto_library(
       name = "nanopb_rpc",
       nanopb_proto_library_deps = [":nanopb"],
       deps = [":proto"],
   )

.. _module-pw_rpc-quickstart-codegen:

2. RPC code generation
======================
How do you interact with this protobuf from C++? ``pw_rpc`` uses
codegen libraries to automatically transform ``.proto`` definitions
into C++ header files. These headers are generated in the build
output directory. Their exact location varies by build system and toolchain
but the C++ include path always matches the source declarations in
``proto_library``. The ``.proto`` extension is replaced with an extension
corresponding to the protobuf codegen library in use:

.. csv-table::
   :header: "Protobuf libraries", "Build subtarget", "Protobuf header", "pw_rpc header"

   "Raw only", "``.raw_rpc``", "(none)", "``.raw_rpc.pb.h``"
   "Nanopb or raw", "``.nanopb_rpc``", "``.pb.h``", "``.rpc.pb.h``"
   "pw_protobuf or raw", "``.pwpb_rpc``", "``.pwpb.h``", "``.rpc.pwpb.h``"

Most projects should default to Nanopb. See
:ref:`module-pw_rpc-guides-headers`.

For example, the generated RPC header for ``applications/blinky.proto`` is
``applications/blinky.rpc.pb.h`` for Nanopb or
``applications/blinky.raw_rpc.pb.h`` for raw RPCs.

The generated header defines a base class for each RPC service declared in the
``.proto`` file. A service named ``TheService`` in package ``foo.bar`` would
generate the following base class for ``pw_protobuf``:

.. cpp:class:: template <typename Implementation> foo::bar::pw_rpc::pwpb::TheService::Service

3. RPC service definition
=========================
Implement a service class by inheriting from the generated RPC service
base class and defining a method for each RPC. The methods must match the name
and function signature for one of the supported protobuf implementations.
Services may mix and match protobuf implementations within one service.

A Nanopb implementation of the ``Blinky`` service looks like this:

.. code-block:: cpp

   /* //applications/blinky/main.cc */

   // ...
   #include "applications/blinky/blinky.rpc.pb.h"
   #include "pw_system/rpc_server.h"
   // ...

   class BlinkyService final
       : public blinky::pw_rpc::nanopb::Blinky::Service<BlinkyService> {
    public:
     pw::Status ToggleLed(const pw_protobuf_Empty&, pw_protobuf_Empty&) {
       // Turn the LED off if it's currently on and vice versa
     }
     pw::Status Blink(const blinky_BlinkRequest& request, pw_protobuf_Empty&) {
       if (request.blink_count == 0) {
         // Stop blinking
       }
       if (request.interval_ms > 0) {
         // Change the blink interval
       }
       if (request.has_blink_count) {  // Auto-generated property
         // Blink request.blink_count times
       }
     }
   };

   BlinkyService blinky_service;

   namespace pw::system {

   void UserAppInit() {
     // ...
     pw::system::GetRpcServer().RegisterService(blinky_service);
   }

   }  // namespace pw::system

Declare the implementation in ``BUILD.bazel``:

.. code-block:: python

   # //applications/blinky/BUILD.bazel

   cc_binary(
       name = "blinky",
       srcs = ["main.cc"],
       deps = [
           ":nanopb_rpc",
           # ...
           "@pigweed//pw_system",
       ],
   )

.. tip::

   The generated code includes RPC service implementation stubs. You can
   reference or copy and paste these to get started with implementing a service.
   These stub classes are generated at the bottom of the pw_rpc proto header.

   To use the stubs, do the following:

   #. Locate the generated RPC header in the build directory. For example:

      .. code-block:: console

         $ cd bazel-out && find . -name blinky.rpc.pb.h

   #. Scroll to the bottom of the generated RPC header.
   #. Copy the stub class declaration to a header file.
   #. Copy the member function definitions to a source file.
   #. Rename the class or change the namespace, if desired.
   #. List these files in a build target with a dependency on
      ``proto_library``.

4. Register the service with a server
=====================================
Set up the server and register the service:

.. code-block:: cpp

   /* //applications/blinky/main.cc */

   // ...
   #include "pw_system/rpc_server.h"
   // ...

   namespace pw::system {

   void UserAppInit() {
     // ...
     pw::system::GetRpcServer().RegisterService(blinky_service);
   }

   }  // namespace pw::system

Next steps
==========
That's the end of the quickstart! Learn more about ``pw_rpc``:

* Check out :ref:`module-pw_rpc-cpp` for detailed guidance on using the C++
  client and server libraries.

* If you have any questions, you can talk to the Pigweed team in the ``#pw_rpc``
  channel of our `Discord <https://discord.gg/M9NSeTA>`_.

* The rest of this page provides general guidance on common questions and use cases.

* The quickstart code was based off these real-world examples of the Pigweed
  team adding ``pw_rpc`` to a project:

  * `applications/blinky: Add Blinky RPC service <https://pwrev.dev/218225>`_

  * `rpc: Use nanopb instead of pw_protobuf <https://pwrev.dev/218732>`_

* You can build clients in other languages, such as Python and TypeScript.
  See :ref:`module-pw_rpc-libraries`.

.. _module-pw_rpc-zephyr:

---------------------------
Setting up pw_rpc in Zephyr
---------------------------
To enable ``pw_rpc.*`` for Zephyr add ``CONFIG_PIGWEED_RPC=y`` to the project's
configuration. This will enable the Kconfig menu for the following:

* ``pw_rpc.server`` which can be enabled via ``CONFIG_PIGWEED_RPC_SERVER=y``.
* ``pw_rpc.client`` which can be enabled via ``CONFIG_PIGWEED_RPC_CLIENT=y``.
* ``pw_rpc.client_server`` which can be enabled via
  ``CONFIG_PIGWEED_RPC_CLIENT_SERVER=y``.
* ``pw_rpc.common`` which can be enabled via ``CONFIG_PIGWEED_RPC_COMMON=y``.

.. _module-pw_rpc-syntax-versions:

---------------------------
proto2 versus proto3 syntax
---------------------------
Always use proto3 syntax rather than proto2 for new protocol buffers. proto2
protobufs can be compiled for ``pw_rpc``, but they are not as well supported
as proto3. Specifically, ``pw_rpc`` lacks support for non-zero default values
in proto2. When using Nanopb with ``pw_rpc``, proto2 response protobufs with
non-zero field defaults should be manually initialized to the default struct.

In the past, proto3 was sometimes avoided because it lacked support for field
presence detection. Fortunately, this has been fixed: proto3 now supports
``optional`` fields, which are equivalent to proto2 ``optional`` fields.

If you need to distinguish between a default-valued field and a missing field,
mark the field as ``optional``. The presence of the field can be detected
with ``std::optional``, a ``HasField(name)``, or ``has_<field>`` member,
depending on the library.

Optional fields have some overhead. If using Nanopb, default-valued fields
are included in the encoded proto, and the proto structs have a
``has_<field>`` flag for each optional field. Use plain fields if field
presence detection is not needed.

.. code-block:: protobuf

   syntax = "proto3";

   message MyMessage {
     // Leaving this field unset is equivalent to setting it to 0.
     int32 number = 1;

     // Setting this field to 0 is different from leaving it unset.
     optional int32 other_number = 2;
   }

.. _module-pw_rpc-guides-headers:

-----------------------------------------------------------
When to use raw, Nanopb, or pw_protobuf headers and methods
-----------------------------------------------------------
There are three types of generated headers and methods available:

* Raw
* Nanopb
* ``pw_protobuf``

This section explains when to use each one. See
:ref:`module-pw_rpc-quickstart-codegen` for context.

``pw_rpc`` doesn't generate raw headers unless you specifically request them
in your build. These headers allow you to use raw methods. Raw methods only
give you a serialized request buffer and an output buffer. Projects typically
only work with raw headers and methods when they have large, complex proto
definitions (e.g. lots of callbacks) that are difficult to work with. Advanced
projects might use raw headers and methods when they need finer control over
how a proto is encoded.

Nanopb and ``pw_protobuf`` are higher-level libraries that make it easier
to serialize or deserialize protos inside raw bytes. Most new projects should
default to Nanopb for the time being. Pigweed has plans to improve ``pw_protobuf``
but those plans will take a while to implement.

The Nanopb and ``pw_protobuf`` APIs and codegen are both built on top of the
underlying raw APIs, which is why it's always possible to fallback to
raw APIs. If you define a Nanopb or ``pw_protobuf`` service, you can choose to
make individual methods raw by defining them using the raw method signature.
You still import the Nanopb or ``pw_protobuf`` header and can use the
methods from those libraries elsewhere. Unless you believe your entire service
requires pure raw methods, it's better to use Nanopb or ``pw_protobuf`` for
most things and fallback to raw only when needed.

.. caution:: Mixing Nanopb and pw_protobuf within the same service not supported

   You can have a mix of Nanopb, ``pw_protobuf``, and raw services on the
   same server. Within a service, you can mix raw and Nanopb or raw and ``pw_protobuf``
   methods. You can't currently mix Nanopb and ``pw_protobuf`` methods but Pigweed
   can implement this if needed. :bug:`234874320` outlines some conflicts you may
   encounter if you try to include Nanopb and ``pw_protobuf`` headers in the
   same source file.

.. _module-pw_rpc-guides-raw-fallback:

Falling back to raw methods
===========================
When implementing an RPC service using Nanopb or ``pw_protobuf``, you may
sometimes run into limitations of the protobuf library when used in conjunction
with ``pw_rpc``. For example, fields which use callbacks require those callbacks
to be set prior to the decode operation, but ``pw_rpc`` internally decodes every
message passed into a method implementation without any opportunity to set
these. Alternatively, you may simply want finer control over how your messages
are encoded.

To assist with these cases, ``pw_rpc`` allows any method within a Nanopb or
``pw_protobuf`` service to use its raw APIs without having to define the entire
service as raw. Implementors may choose on a method-by-method basis where they
desire to have access to the raw protobuf messages.

To implement a method using the raw APIs, all you have to do is change the
signature of the function --- ``pw_rpc`` will automatically handle the rest.
Examples are provided below, each showing a Nanopb method and its equivalent
raw signature.

Unary method
------------
When defining a unary method using the raw APIs, it is important to note that
there is no synchronous raw unary API. The asynchronous unary method signature
must be used instead.

**Nanopb**

.. code-block:: c++

   // Synchronous unary method.
   pw::Status DoFoo(const FooRequest& request, FooResponse response);

   // Asynchronous unary method.
   void DoFoo(const FooRequest& request,
              pw::rpc::NanopbUnaryResponder<FooResponse>& responder);

**Raw**

.. code-block:: c++

   // Only asynchronous unary methods are supported.
   void DoFoo(pw::ConstByteSpan request, pw::rpc::RawUnaryResponder& responder);

Server streaming method
-----------------------

**Nanopb**

.. code-block:: c++

   void DoFoo(const FooRequest& request,
              pw::rpc::NanopbServerWriter<FooResponse>& writer);

**Raw**

.. code-block:: c++

   void DoFoo(pw::ConstByteSpan request, pw::rpc::RawServerWriter& writer);

Client streaming method
-----------------------

**Nanopb**

.. code-block:: c++

   void DoFoo(pw::rpc::NanopbServerReader<FooRequest, FooResponse>&);

**Raw**

.. code-block:: c++

   void DoFoo(RawServerReader&);

Bidirectional streaming method
------------------------------

**Nanopb**

.. code-block:: c++

   void DoFoo(pw::rpc::NanopbServerReaderWriter<Request, Response>&);

**Raw**

.. code-block:: c++

   void DoFoo(RawServerReaderWriter&);

----------------------------
Testing a pw_rpc integration
----------------------------
After setting up a ``pw_rpc`` server in your project, you can test that it is
working as intended by registering the provided ``EchoService``, defined in
``echo.proto``, which echoes back a message that it receives.

.. literalinclude:: echo.proto
   :language: protobuf
   :lines: 14-

For example, in C++ with pw_protobuf:

.. code-block:: c++

   #include "pw_rpc/server.h"

   // Include the apporpriate header for your protobuf library.
   #include "pw_rpc/echo_service_pwpb.h"

   constexpr pw::rpc::Channel kChannels[] = {/* ... */};
   static pw::rpc::Server server(kChannels);

   static pw::rpc::EchoService echo_service;

   void Init() { server.RegisterService(echo_service); }

See :ref:`module-pw_rpc-cpp-testing` for more C++-specific testing
guidance.

-------------------------------
Benchmarking and stress testing
-------------------------------
``pw_rpc`` provides an RPC service and Python module for stress testing and
benchmarking a ``pw_rpc`` deployment.

pw_rpc provides tools for stress testing and benchmarking a Pigweed RPC
deployment and the transport it is running over. Two components are included:

* The pw.rpc.Benchmark service and its implementation.
* A Python module that runs tests using the Benchmark service.

pw.rpc.Benchmark service
========================
The Benchmark service provides a low-level RPC service for sending data between
the client and server. The service is defined in ``pw_rpc/benchmark.proto``.

A raw RPC implementation of the benchmark service is provided. This
implementation is suitable for use in any system with pw_rpc. To access this
service, add a dependency on ``"$dir_pw_rpc:benchmark"`` in GN or
``pw_rpc.benchmark`` in CMake. Then, include the service
(``#include "pw_rpc/benchmark.h"``), instantiate it, and register it with your
RPC server, like any other RPC service.

The Benchmark service was designed with the Python-based benchmarking tools in
mind, but it may be used directly to test basic RPC functionality. The service
is well suited for use in automated integration tests or in an interactive
console.

Benchmark service
-----------------
.. literalinclude:: benchmark.proto
  :language: protobuf
  :lines: 14-

Example
-------
.. code-block:: c++

   #include "pw_rpc/benchmark.h"
   #include "pw_rpc/server.h"

   constexpr pw::rpc::Channel kChannels[] = {/* ... */};
   static pw::rpc::Server server(kChannels);

   static pw::rpc::BenchmarkService benchmark_service;

   void RegisterServices() { server.RegisterService(benchmark_service); }

Stress testing
==============
.. attention::
   This section is experimental and liable to change.

The Benchmark service is also used as part of a stress test of the ``pw_rpc``
module. This stress test is implemented as an unguided fuzzer that uses
multiple worker threads to perform generated sequences of actions using RPC
``Call`` objects. The test is included as an integration test, and can found and
be run locally using GN:

.. code-block:: bash

   $ gn desc out //:integration_tests deps | grep fuzz
   //pw_rpc/fuzz:cpp_client_server_fuzz_test(//targets/host/pigweed_internal:pw_strict_host_clang_debug)

   $ gn outputs out '//pw_rpc/fuzz:cpp_client_server_fuzz_test(//targets/host/pigweed_internal:pw_strict_host_clang_debug)'
   pw_strict_host_clang_debug/gen/pw_rpc/fuzz/cpp_client_server_fuzz_test.pw_pystamp

   $ ninja -C out pw_strict_host_clang_debug/gen/pw_rpc/fuzz/cpp_client_server_fuzz_test.pw_pystamp
