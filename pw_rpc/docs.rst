.. _module-pw_rpc:

======
pw_rpc
======
.. pigweed-module::
   :name: pw_rpc

.. tab-set::


   .. tab-item:: blinky.proto

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

   .. tab-item:: main.cc

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

   .. tab-item:: BUILD.bazel

      .. code-block:: python

         # //applications/blinky/BUILD.bazel

         load(
             "@pigweed//pw_protobuf_compiler:pw_proto_library.bzl",
             "nanopb_proto_library",
             "nanopb_rpc_proto_library",
         )

          cc_binary(
              name = "blinky",
              srcs = ["main.cc"],
              deps = [
                  ":nanopb_rpc",
                  # ...
                  "@pigweed//pw_system",
              ],
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

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart & guides
      :link: module-pw_rpc-guides
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Check out the ``pw_rpc`` quickstart for more explanation of the code above.
      The guides answer common questions such as whether
      to use ``proto2`` or ``proto3`` syntax.

   .. grid-item-card:: :octicon:`code-square` C++ server and client
      :link: module-pw_rpc-cpp
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      C++ server and client library API guides.

.. grid:: 2

   .. grid-item-card:: :octicon:`info` Packet protocol
      :link: module-pw_rpc-protocol
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      A detailed description of the ``pw_rpc`` packet protocol.

   .. grid-item-card:: :octicon:`info` Design
      :link: module-pw_rpc-design
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      An overview of the RPC call lifecycle, naming conventions,
      and the ``pw_rpc`` roadmap.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Python client
      :link: module-pw_rpc-py
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Python client library API reference.

   .. grid-item-card:: :octicon:`code-square` TypeScript client
      :link: module-pw_rpc-ts
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      TypeScript client library API guide.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Nanopb codegen
      :link: module-pw_rpc_nanopb
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Nanopb codegen library API guide.

   .. grid-item-card:: :octicon:`code-square` pw_protobuf codegen
      :link: module-pw_rpc_pw_protobuf
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      ``pw_protobuf`` codegen library API guide.

.. toctree::
   :maxdepth: 1
   :hidden:

   guides
   libraries
   protocol
   design
   HDLC example <../pw_hdlc/rpc_example/docs>
