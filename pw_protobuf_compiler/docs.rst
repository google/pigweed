.. _module-pw_protobuf_compiler:

--------------------
pw_protobuf_compiler
--------------------
The Protobuf compiler module provides build system integration and wrapper
scripts for generating source code for Protobuf definitions.

Generator support
=================
Protobuf code generation is currently supported for the following generators:

+-------------+----------------+-----------------------------------------------+
| Generator   | Code           | Notes                                         |
+-------------+----------------+-----------------------------------------------+
| pw_protobuf | ``pwpb``       | Compiles using ``pw_protobuf``.               |
+-------------+----------------+-----------------------------------------------+
| Nanopb      | ``nanopb``     | Compiles using Nanopb. The build argument     |
|             |                | ``dir_pw_third_party_nanopb`` must be set to  |
|             |                | point to a local nanopb installation.         |
+-------------+----------------+-----------------------------------------------+
| Nanopb RPC  | ``nanopb_rpc`` | Compiles pw_rpc service and client code for   |
|             |                | nanopb. Requires a nanopb installation.       |
+-------------+----------------+-----------------------------------------------+
| Raw RPC     | ``raw_rpc``    | Compiles raw binary pw_rpc service code.      |
+-------------+----------------+-----------------------------------------------+
| Go          | ``go``         | Compiles using the standard Go protobuf       |
|             |                | plugin with gRPC service support.             |
+-------------+----------------+-----------------------------------------------+
| Python      | ``python``     | Compiles using the standard Python protobuf   |
|             |                | plugin, creating a ``pw_python_package``.     |
+-------------+----------------+-----------------------------------------------+

GN template
===========
The ``pw_proto_library`` GN template is provided by the module.

It defines a collection of protobuf files that should be compiled together. The
template creates a sub-target for each supported generator, named
``<target_name>.<generator>``. These sub-targets generate their respective
protobuf code, and expose it to the build system appropriately (e.g. a
``pw_source_set`` for C/C++).

For example, given the following target:

.. code-block::

  pw_proto_library("test_protos") {
    sources = [ "my_test_protos/test.proto" ]
  }

``test_protos.pwpb`` compiles code for pw_protobuf, and ``test_protos.nanopb``
compiles using Nanopb (if it's installed).

Protobuf code is only generated when a generator sub-target is listed as a
dependency of another GN target.

**Arguments**

* ``sources``: List of ``.proto`` files.
* ``deps``: Other ``pw_proto_library`` targets that this one depends on.

**Example**

.. code::

  import("$dir_pw_protobuf_compiler/proto.gni")

  pw_proto_library("my_protos") {
    sources = [
      "my_protos/foo.proto",
      "my_protos/bar.proto",
    ]
  }

  pw_proto_library("my_other_protos") {
    sources = [ "my_other_protos/baz.proto" ]  # imports foo.proto

    # Proto libraries depend on other proto libraries directly.
    deps = [ ":my_protos" ]
  }

  source_set("my_cc_code") {
    sources = [
      "foo.cc",
      "bar.cc",
      "baz.cc",
    ]

    # When depending on protos in a source_set, specify the generator suffix.
    deps = [ ":my_other_protos.pwpb" ]
  }

Proto file structure
--------------------
Protobuf source files must be nested under another directory when they are
listed in sources. This ensures that they can be packaged properly in Python.
The first directory is used as the Python package name.

The requirements for proto file structure in the source tree will be relaxed in
future updates.

Working with externally defined protos
--------------------------------------
``pw_proto_library`` targets may be used to build ``.proto`` sources from
existing projects. In these cases, it may be necessary to supply the
``include_path`` argument, which specifies the protobuf include path to use for
``protoc``. If only a single external protobuf is being compiled, the
requirement that the protobuf be nested under a directory is waived. This
exception should only be used when absolutely necessary -- for example, to
support proto files that includes ``import "nanopb.proto"`` in them.
