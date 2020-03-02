.. default-domain:: py
.. highlight:: py

.. _chapter-pw-protobuf-compiler:

--------------------
pw_protobuf_compiler
--------------------

The Protobuf compiler module provides build system integration and wrapper
scripts for generating source code for Protobuf definitions.

Language support
================

Protobuf code generation is currently supported for the following languages:

+----------+--------+----------------------------------------------------------+
| Language | Code   | Notes                                                    |
+----------+--------+----------------------------------------------------------+
| C++      | ``cc`` | Currently only supports using ``pw_protobuf``.           |
|          |        |                                                          |
|          |        | Support for other libraries such as ``nanopb`` is        |
|          |        | planned.                                                 |
+----------+--------+----------------------------------------------------------+
| Go       | ``go`` | Compiles using the standard Go protobuf plugin with gRPC |
|          |        | service support.                                         |
+----------+--------+----------------------------------------------------------+

The build variable ``pw_protobuf_langs`` tells the module the languages for
which it should compile code. It is defined as a list of language codes.

GN template
===========

The ``pw_proto_library`` GN template is provided by the module.

It tells the build system to compile a set of source proto files to a library in
each chosen language. A different target is created for each language, with the
language's code appended as a suffix to the template's target name.

For example, given the definitions:

.. code::

  pw_protobuf_langs = [ "cc", "py" ]

  pw_proto_library("test_protos") {
    sources = [ "test.proto" ]
  }

Two targets are created, named ``test_protos_cc`` and ``test_protos_py``,
containing the generated code for their respective languages.

**Arguments**

* ``sources``: List of ``.proto`` files.
* ``deps``: Other ``pw_proto_library`` targets that this one depends on.

**Example**

.. code::

  import("$dir_pw_protobuf_compiler/proto.gni")

  pw_proto_library("my_protos") {
    sources = [
      "foo.proto",
      "bar.proto",
    ]
  }

  pw_proto_library("my_other_protos") {
    sources = [
      "baz.proto", # imports foo.proto
    ]
    deps = [
      ":my_protos",
    ]
  }

  source_set("my_cc_code") {
    sources = [
      "foo.cc",
      "bar.cc",
      "baz.cc",
    ]
    deps = [
      ":my_other_protos_cc",
    ]
  }
