.. _module-pw_snapshot-module_usage:

============
Module Usage
============
Right now, pw_snapshot just dictates a *format*. That means there is no provided
system information collection integration, underlying storage, or transport
mechanism to fetch a snapshot from a device. These must be set up independently
by your project.

-------------------
Building a Snapshot
-------------------
Even though a Snapshot is just a proto message, the potential size of the proto
makes it important to consider the encoder.

Nanopb is a popular encoder for embedded devices, it's impractical to use
with the pw_snapshot proto. Nanopb works by generating in-memory structs that
represent the protobuf message. Repeated, optional, and variable-length fields
increase the size of the in-memory struct. The struct representation
of snapshot-like protos can quickly near 10KB in size. Allocating 10KB

Pigweed's pw_protobuf is a better choice as its design is centered around
incrementally writing a proto directly to the final wire format. If you only
write a few fields in a snapshot, you can do so with minimal memory overhead.

.. code-block:: cpp

  #include "pw_protobuf/encoder.h"
  #include "pw_snapshot_protos/snapshot.pwpb.h"

  Result<ConstByteSpan> EncodeSnapshot(pw::ByteSpan encode_buffer,
                                       const CrashInfo &crash_info) {
    // Instantiate a generic proto encoder.
    pw::protobuf::NestedEncoder<kMaxNestedProtoDepth> proto_encoder(
        encode_buffer);
    // Get a proto-specific wrapper for the encoder.
    pw::snapshot::Snapshot::Encoder snapshot_encoder(&proto_encoder);
    {  // This scope is required to handle RAII behavior of the submessage.
      // Start writing the Metadata submessage.
      pw::snapshot::Metadata::Encoder metadata_encoder =
          snapshot_encoder.GetMetadataEncoder();
      metadata_encoder.WriteReason(EncodeReasonLog(crash_info));
      metadata_encoder.WriteFatal(true);
      metadata_encoder.WriteProjectName(std::as_bytes(std::span("smart-shoe")));
      metadata_encoder.WriteDeviceName(
          std::as_bytes(std::span("smart-shoe-p1")));
      metadata_encoder.WriteUptime(
          std::chrono::time_point_cast<std::chrono::milliseconds>(
              pw::chrono::SystemClock::now()));
    }
    // Finalize the proto encode so it can be flushed somewhere.
    return proto_encoder.Encode();
  }

-------------------
Custom Project Data
-------------------
There are two main ways to add custom project-specific data to a snapshot. Tags
are the simplest way to capture small snippets of information that require
no or minimal post-processing. For more complex data, it's usually more
practical to extend the Snapshot proto.

Tags
====
Adding a key/value pair to the tags map is straightforward when using
pw_protobuf.

.. code-block:: cpp

  {
    pw::Snapshot::TagsEntry::Encoder tags_encoder =
        snapshot_encoder.GetTagsEncoder();
    tags_encoder.WriteKey("BtState");
    tags_encoder.WriteValue("connected");
  }

Extending the Proto
===================
Extending the Snapshot proto relies on proto behavior details that are explained
in the :ref:`Snapshot Proto Format<module-pw_snapshot-proto_format>`. Extending
the snapshot proto is as simple as defining a proto message that **only**
declares fields with numbers that are reserved by the Snapshot proto for
downstream projects. When encoding your snapshot, you can then write both the
upstream Snapshot proto and your project's custom extension proto message to the
same proto encoder.

The upstream snapshot tooling will ignore any project-specific proto data,
the proto data can be decoded a second time using a project-specific proto. At
that point, any handling logic of the project-specific data would have to be
done as part of project-specific tooling.

-------------------
Analyzing Snapshots
-------------------
Snapshots can be processed for analysis using the ``pw_snapshot.process`` python
tool. This tool turns a binary snapshot proto into human readable, actionable
information. As some snapshot fields may optionally be tokenized, a
pw_tokenizer database or ELF file with embedded pw_tokenizer tokens may
optionally be passed to the tool to detokenize applicable fields.

.. code-block:: sh

  # Example invocation, which dumps to stdout by default.
  $ python -m pw_snapshot.processor path/to/serialized_snapshot.bin


          ____ _       __    _____ _   _____    ____  _____ __  ______  ______
         / __ \ |     / /   / ___// | / /   |  / __ \/ ___// / / / __ \/_  __/
        / /_/ / | /| / /    \__ \/  |/ / /| | / /_/ /\__ \/ /_/ / / / / / /
       / ____/| |/ |/ /    ___/ / /|  / ___ |/ ____/___/ / __  / /_/ / / /
      /_/     |__/|__/____/____/_/ |_/_/  |_/_/    /____/_/ /_/\____/ /_/
                    /_____/


                              ▪▄▄▄ ▄▄▄· ▄▄▄▄▄ ▄▄▄· ▄ ·
                              █▄▄▄▐█ ▀█ • █▌ ▐█ ▀█ █
                              █ ▪ ▄█▀▀█   █. ▄█▀▀█ █
                              ▐▌ .▐█ ▪▐▌ ▪▐▌·▐█ ▪▐▌▐▌
                              ▀    ▀  ▀ ·  ▀  ▀  ▀ .▀▀

  Device crash cause:
      Assert failed: 1+1 == 42

  Project name:      gShoe
  Device:            GSHOE-QUANTUM_CORE-REV_0.1
  Device FW version: QUANTUM_CORE-0.1.325-e4a84b1a
  FW build UUID:     ad2d39258c1bc487f07ca7e04991a836fdf7d0a0
  Snapshot UUID:     8481bb12a162164f5c74855f6d94ea1a

