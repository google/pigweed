.. _module-pw_kvs-disk-format:

=======================
On-Disk Format & Design
=======================
The ``pw_kvs`` module stores data as a log of entries written sequentially into
flash sectors. This append-only design enables safe recovery from unexpected
power loss. The system is designed primarily for
`NOR flash <https://en.wikipedia.org/wiki/Flash_memory#Distinction_between_NOR_and_NAND_flash>`_,
which is common in embedded systems. It does not include features required for
NAND flash, such as bad block management, as these are typically handled by a
dedicated flash translation layer (FTL).

.. _module-pw_kvs-disk-format-log-structured:

High-Level Structure: A Log of Entries in Sectors
=================================================
The flash memory managed by ``pw_kvs`` is divided into sectors. The KVS operates
as a log-structured file system, meaning data is only ever appended to the log;
it is never modified in place.

Entries are written one after another into the current active sector. When a key
is updated, a new entry containing the new value is written to the end of the
log. The previous entry for that key is now considered "stale." Deletes are
handled similarly, by writing a special "tombstone" entry.

This append-only approach was selected due to its suitabiily to flash memory,
which can be written to in small increments but must be erased in larger blocks.
When a sector fills up with entries (including stale ones),
:ref:`garbage collection <module-pw_kvs-guides-garbage-collection>` is
triggered to reclaim the space.

.. _module-pw_kvs-disk-format-entry-structure:

Low-Level Structure: The Entry Format
=====================================
Each piece of data in the KVS is stored in an "entry" packet. This packet
contains metadata alongside the key and value. The structure is designed to be
compact and efficient for embedded systems.

.. list-table:: KVS Entry Structure
   :widths: 20 15 65
   :header-rows: 1

   * - Field
     - Size
     - Description
   * - Magic
     - 32-bit
     - An identifier that marks the beginning of a valid entry and indicates the
       data format version.
   * - Checksum
     - Variable
     - A checksum (e.g. CRC16) of the entry's contents, used to verify data
       integrity. The size is determined by the ``ChecksumAlgorithm`` configured
       for the KVS.
   * - Alignment
     - 8-bit
     - An 8-bit field (`alignment_units`) from which the entry's alignment is
       calculated. The alignment in bytes is ``(alignment_units + 1) * 16``.
       This allows for alignments from 16 to 4096 bytes.
   * - Key Length
     - 8-bit
     - The length of the key string in bytes.
   * - Value Size
     - 16-bit
     - The size of the value in bytes. A special value of ``0xFFFF`` marks the
       entry as a "tombstone," indicating the key has been deleted.
   * - Transaction ID
     - 32-bit
     - A monotonically increasing number that orders the entries.
   * - Key
     - Variable
     - The key string, with a variable length up to 255 bytes. It is not
       null-terminated.
   * - Value
     - Variable
     - The data associated with the key.
   * - Padding
     - Variable
     - Zero-bytes added to the end of the entry so that its total size is a
       multiple of the entry's calculated alignment.

.. _module-pw_kvs-disk-format-invariants:

Design principle: no global metadata
====================================
Many storage systems maintain a central block of metadata (e.g., allocation
tables, journals) that contains information about the overall state of the
system. While this can offer performance benefits, it also introduces a single
point of failure.

``pw_kvs`` avoids this by deriving its state entirely from the individual
key-value entries stored in flash. This design has several advantages:

- **Robustness**: There is no single block of data that can be corrupted and
  render the entire KVS unusable. There is also no single block that must be
  repeatedly modified, which would lead to flash degredation.
- **Simplicity**: The logic for the KVS is simplified, since there aren't
  complicated invariants maintained between the entries and a global metadata
  block.

This approach means that the KVS can be initialized by simply scanning the
entire flash partition and reading all valid entries. The tradeoff for avoiding
global metadata is that the KVS can't easily support multi-key transactions,
where values for multiple keys are modified, and either all the updates are
recorded (in a successful write) or none are (in a data loss event). This
tradeoff is sufficient for common embedded use cases.

Invariants
==========
The KVS maintains the following invariants.

- **One Free Sector for GC**: The KVS requires at least one free (erased)
  sector at all times to guarantee that garbage collection can always proceed.

- **Entries Do Not Cross Sector Boundaries**: An entry must be fully contained
  within a single sector. This is a direct consequence of the physical
  constraints of flash hardware, which can only be erased in fixed-size blocks
  (sectors). If an entry spanned two sectors, erasing one would corrupt the
  entry.

- **Self-Contained Entries**: Every entry is a complete, self-describing
  record. It contains all the information needed to interpret its own data,
  including its size (via alignment), key length, and value size.

- **Redundant copies in different sectors**: The KVS maintains a configured
  number of copies (replicas) for each logical key-value entry. Each of these
  entries is bit-for-bit identical, including the key, value, and transaction
  ID. As a placement heuristic, each copy is stored in a different flash sector
  to protect against sector-level corruption. If a corruption event or flash
  failure leads to the loss of one or more copies, the KVS detects this deficit
  and restores the configured level of redundancy.

- **Highest Transaction ID is Newest**: For a given key, the entry with the
  highest transaction ID is the most recent one.

Implementation Details
======================

Alignment, Padding, and Forward Compatibility
---------------------------------------------
The ``Alignment`` and ``Padding`` fields work together to ensure that each entry
is stored correctly on flash memory, which often has specific alignment
requirements for writes.

The key design choice is that the required alignment is stored **within each
entry's header**. This 8-bit value isn't the alignment itself, but an
``alignment_units`` value from which the byte alignment is calculated using the
formula ``(alignment_units + 1) * 16``. Advantages of this approach:

- **Flexibility**: It supports alignments from 16 bytes (for ``alignment_units
  = 0``) to 4096 bytes (for ``alignment_units = 255``), which is sufficient for
  most flash hardware.
- **Space Efficiency**: Storing a single 8-bit integer is more compact than
  storing a 16-bit or 32-bit alignment value.
- **Forward Compatibility**: Because each entry is self-describing, a future
  firmware update can change the alignment used for new entries. The new
  firmware can still correctly calculate the size of older entries. When an old
  entry is moved during garbage collection, it is rewritten with the new, larger
  alignment, upgrading the data in place over time.

Transaction ID Rollover
-----------------------
To identify the most recent version of a key, the KVS uses a 32-bit transaction
ID that is incremented on every write.

By design, the KVS does not handle the rollover of this transaction ID. This is a
practical trade-off made based on the lifecycle of typical flash hardware. A
32-bit transaction ID provides approximately 4.3 billion unique IDs. In
contrast, a standard NOR flash sector has a write/erase endurance of around
100,000 cycles.

Because ``pw_kvs`` wear-levels by distributing writes across all available
sectors, the physical flash memory is expected to wear out long before the
transaction ID space is exhausted. For most embedded applications, this makes
transaction ID rollover a theoretical concern rather than a practical one.
