.. _docs-pw-style-protobuf:

==============
Protobuf style
==============
The Pigweed protobuf style guide is closely based on `Google's public
protobuf style guide <https://protobuf.dev/programming-guides/style/>`_.
The Google Protobuf Style Guide applies to Pigweed except as described in this
document.

The Pigweed protobuf style guide only applies to Pigweed itself. It does not
apply to projects that use Pigweed or to the third-party code included with
Pigweed.

--------
Editions
--------
Pigweed only allows ``proto3`` (i.e. ``syntax = "proto3";``) at this time.
``proto2`` is not supported, and the ``edition`` versions (e.g.
``edition = "2023";``) will be considered when sufficient motivation arises.

------
Naming
------
Protobuf library structure
==========================
All protobuf files should live in a ``proto`` subdirectory within their
respective modules. This produces an ``import`` path that matches the pattern of
``[module name]/proto/[proto file].proto``\. If a module exposes multiple proto
libraries that need to be grouped separately from a build/distribution
perspective, additional directories may be introduced that follow the pattern
``[module name]/[group name]_proto/[proto file].proto``\.

Examples:

* ``pw_log/proto``
* ``pw_unit_test/proto``
* ``pw_snapshot/proto``
* ``pw_snapshot/metadata_proto``

Rationale: This overlays the include paths of native libraries, but introduces
``proto`` to the enclosing directory hierarchy to make it clearer that generated
proto libraries are being introduced.

Protobuf package names
======================
Protobuf library packages should append a ``proto`` suffix to package
namespaces. If a module exposes multiple proto
libraries that need to be grouped separately from a build/distribution
perspective, a ``*_proto`` suffix may be used instead.

Example:

.. code-block:: protobuf
   :emphasize-lines: 3

   // This lives inside of pw_file.

   package pw.file.proto;

   service FileSystem {
     // ...
   }

Rationale: This prevents collisions between generated proto types and
language-native types.

RPC service names
=================
Services should strive for simple, intuitive, globally unique names, and should
**not** be suffixed with ``*Service``.

Example:

.. code-block:: protobuf
   :emphasize-lines: 1

   service PwEcho {
     rpc Echo(EchoRequest) returns (EchoResponse) {}
   }

Rationale: Pigweed's C++ RPC codegen namespaces services differently than
regular protos (for example, ``pw::file::pw_rpc::raw::FileSystem::Service``),
which makes it sufficiently clear when a name refers to a service.

------
Typing
------
Using ``optional``
==================
When presence or absence of a field's value has a semantic meaning, the field
should be marked as ``optional`` to signal the distinction. Otherwise,
``optional`` should be omitted. This causes the field to still be optional, but
indicates that the default value of zero is semantically equivalent to an
explicit value of zero.

Example:

.. code-block:: protobuf
   :emphasize-lines: 6

   message MyRequest {
     // The maximum number of `foo` entries to return in the response message.
     // If this is not set, the response may contain as many `foo` entries
     // as needed. If `max_foo` is zero, no `foo` entries should be included
     // in the response.
     optional uint32 max_foo = 1;
   }

Rationale: ``optional`` is very useful for clarifying cases where an implied
default value seems ambiguous, and it also signals codegen for ``has_max_foo``
getters in various languages.

Using ``required``
==================
``required`` fields are strictly forbidden within Pigweed.

-----
Enums
-----
Default values
==============
All enums must have a safe default zero value. Usually this is ``UNKNOWN`` or
``NONE``, but may be any other semantically similar default.

Rationale: Enums are default-initialized to zero, which means that a zero value
that conveys anything beyond "unset" may be misleading.

.. code-block:: protobuf
   :emphasize-lines: 5

   package pw.chrono.proto;

   message EpochType {
     enum Enum {
       UNKNOWN = 0;
       TIME_SINCE_BOOT = 1;
       UTC_WALL_CLOCK = 2;
       GPS_WALL_CLOCK = 3;
       TAI_WALL_CLOCK = 4;
     };
   }

Namespacing
===========
Prefer to place ``enum`` definitions within a ``message`` to namespace the
generated names for the values.

Rationale: Enum value names can easily collide if you don't prefix them (i.e.
``UNKNOWN`` from one enum will collide with ``UNKNOWN`` from another enum).
Namespacing them within a message prevents these collisions.

.. code-block:: protobuf
   :emphasize-lines: 3, 4

   package pw.chrono.proto;

   message EpochType {
     enum Enum {
       UNKNOWN = 0;
       TIME_SINCE_BOOT = 1;
       UTC_WALL_CLOCK = 2;
       GPS_WALL_CLOCK = 3;
       TAI_WALL_CLOCK = 4;
     };
   }
