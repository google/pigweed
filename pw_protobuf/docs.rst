.. _module-pw_protobuf:

-----------
pw_protobuf
-----------
The protobuf module provides a lightweight interface for encoding and decoding
the Protocol Buffer wire format.

.. note::

  The protobuf module is a work in progress. Wire format encoding and decoding
  is supported, though the APIs are not final. C++ code generation exists for
  encoding, but not decoding.

Design
======
Unlike other protobuf libraries, which typically provide in-memory data
structures to represent protobuf messages, ``pw_protobuf`` operates directly on
the wire format and leaves data storage to the user. This has a few benefits.
The primary one is that it allows the library to be incredibly small, with the
encoder and decoder each having a code size of around 1.5K and negligible RAM
usage. Users can choose the tradeoffs most suitable for their product on top of
this core implementation.

``pw_protobuf`` also provides zero-overhead C++ code generation which wraps its
low-level wire format operations with a user-friendly API for processing
specific protobuf messages. The code generation integrates with Pigweed's GN
build system.

Usage
=====
``pw_protobuf`` splits wire format encoding and decoding operations. Links to
the design and APIs of each are listed in below.

See also :ref:`module-pw_protobuf_compiler` for details on ``pw_protobuf``'s
build system integration.

**pw_protobuf functionality**

.. toctree::
  :maxdepth: 1

  decoding

Comparison with other protobuf libraries
========================================

protobuf-lite
^^^^^^^^^^^^^
protobuf-lite is the official reduced-size C++ implementation of protobuf. It
uses a restricted subset of the protobuf library's features to minimize code
size. However, is is still around 150K in size and requires dynamic memory
allocation, making it unsuitable for many embedded systems.

nanopb
^^^^^^
`nanopb <https://github.com/nanopb/nanopb>`_ is a commonly used embedded
protobuf library with very small code size and full code generation. It provides
both encoding/decoding functionality and in-memory C structs representing
protobuf messages.

nanopb works well for many embedded products; however, using its generated code
can run into RAM usage issues when processing nontrivial protobuf messages due
to the necessity of defining a struct capable of storing all configurations of
the message, which can grow incredibly large. In one project, Pigweed developers
encountered an 11K struct statically allocated for a single message---over twice
the size of the final encoded output! (This was what prompted the development of
``pw_protobuf``.)

To avoid this issue, it is possible to use nanopb's low-level encode/decode
functions to process individual message fields directly, but this loses all of
the useful semantics of code generation. ``pw_protobuf`` is designed to optimize
for this use case; it allows for efficient operations on the wire format with an
intuitive user interface.

Depending on the requirements of a project, either of these libraries could be
suitable.
