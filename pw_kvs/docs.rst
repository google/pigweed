.. _module-pw_kvs:

======
pw_kvs
======
.. pigweed-module::
   :name: pw_kvs

.. tab-set::

   .. tab-item:: app.cpp

      .. code-block:: cpp

         #include <cstddef>

         #include "pw_kvs/flash_test_partition.h"
         #include "pw_kvs/key_value_store.h"
         // Not a required dep; just here for demo comms
         #include "pw_sys_io/sys_io.h"

         // Create our key-value store (KVS). Sector and entry vals for this
         // demo are based on @pigweed//pw_kvs:fake_flash_64_aligned_partition
         constexpr size_t kMaxSectors = 6;
         constexpr size_t kMaxEntries = 64;
         static constexpr pw::kvs::EntryFormat kvs_format = {
           .magic = 0xd253a8a9,  // Prod apps should use a random number here
           .checksum = nullptr
         };
         pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(
           &pw::kvs::FlashTestPartition(),
           kvs_format
         );

         kvs.Init();  // Initialize our KVS
         std::byte in;
         pw::sys_io::ReadByte(&in).IgnoreError();  // Get a char from the user
         kvs.Put("in", in);  // Save the char to our key-value store (KVS)
         std::byte out;
         kvs.Get("in", &out);  // Test that the KVS stored the data correctly
         pw::sys_io::WriteByte(out).IgnoreError();  // Echo the char back out

   .. tab-item:: BUILD.bazel

      .. code-block:: py

         cc_binary(
             name = "app",
             srcs = ["app.cc"],
             # ...
             deps = [
                 # ...
                 "@pigweed//pw_kvs",
                 "@pigweed//pw_kvs:fake_flash_64_aligned_partition",
                 # ...
             ]
             # ...
         )

``pw_kvs`` is a flash-backed, persistent key-value storage (KVS) system with
integrated :ref:`wear leveling <module-pw_kvs-design-wear>`. It's a relatively
lightweight alternative to a file system.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Get started
      :link: module-pw_kvs-start
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Integrate pw_kvs into your project

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: module-pw_kvs-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      pw_kvs API reference

.. grid:: 2

   .. grid-item-card:: :octicon:`stack` Design
      :link: module-pw_kvs-design
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How pw_kvs manages state, does garbage collection, etc.

   .. grid-item-card:: :octicon:`graph` Code size analysis
      :link: module-pw_kvs-size
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      The code size impact of using pw_kvs in your project

.. _module-pw_kvs-start:

-----------
Get started
-----------
.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_kvs`` to your target's ``deps``:

      .. code-block::

         cc_binary(
           # ...
           deps = [
             # ...
             "@pigweed//pw_kvs",
             # ...
           ]
         )

      This assumes that your Bazel ``WORKSPACE`` has a `repository
      <https://bazel.build/concepts/build-ref#repositories>`_ named ``@pigweed``
      that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``$dir_pw_kvs`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_kvs",
             # ...
           ]
         }

   .. tab-item:: CMake

      Link your library to ``pw_kvs``:

      .. code-block::

         add_library(my_lib ...)
         target_link_libraries(my_lib PUBLIC pw_kvs)

Use ``pw_kvs`` in your C++ code:

.. code-block:: c++

   #include "pw_kvs/key_value_store.h"

   // ...

.. _pw_kvs/flash_memory.h: https://cs.opensource.google/pigweed/pigweed/+/main:pw_kvs/public/pw_kvs/flash_memory.h

Implement the :ref:`flash memory <module-pw_kvs-design-memory>` and
:ref:`flash partition <module-pw_kvs-design-partitions>` interfaces for
your hardware. See `pw_kvs/flash_memory.h`_.

.. _module-pw_kvs-reference:

---------
Reference
---------

.. _module-pw_kvs-reference-keyvaluestore:

``pw::kvs::KeyValueStore``
==========================
See :ref:`module-pw_kvs-design` for architectural details.

.. doxygenclass:: pw::kvs::KeyValueStore
   :members:

Configuration
=============
.. doxygendefine:: PW_KVS_LOG_LEVEL
.. doxygendefine:: PW_KVS_MAX_FLASH_ALIGNMENT
.. doxygendefine:: PW_KVS_REMOVE_DELETED_KEYS_IN_HEAVY_MAINTENANCE

.. _module-pw_kvs-design:

------
Design
------
:cpp:class:`pw::kvs::KeyValueStore` ("the KVS") stores key and value data
pairs. The key-value pairs are stored in :ref:`flash partition
<module-pw_kvs-design-memory>` as a :ref:`key-value entry
<module-pw_kvs-design-entries>` (KV entry) that consists of a header/metadata,
the key data, and value data. KV entries are accessed through ``Put()``,
``Get()``, and ``Delete()`` operations.

.. _module-pw_kvs-design-entries:

Key-value entries
=================
Each key-value (KV) entry consists of a header/metadata, the key data, and
value data. Individual KV entries are contained within a single flash sector;
they do not cross sector boundaries. Because of this the maximum KV entry size
is the partition sector size.

KV entries are appended as needed to sectors, with append operations spread
over time. Each individual KV entry is written completely as a single
high-level operation. KV entries are appended to a sector as long as space is
available for a given KV entry. Multiple sectors can be active for writing at
any time.

When an entry is updated, an entirely new entry is written to a new location
that may or may not be located in the same sectore as the old entry. The new
entry uses a transaction ID greater than the old entry. The old entry remains
unaltered "on-disk" but is considered "stale". It is :ref:`garbage collected
<module-pw_kvs-design-garbage>` at some future time.

.. _module-pw_kvs-design-state:

State
=====
The KVS does not store any data/metadata/state in flash beyond the KV
entries. All KVS state can be derived from the stored KV entries.
Current state is determined at boot from flash-stored KV entries and
then maintained in RAM by the KVS. At all times the KVS is in a valid state
on-flash; there are no windows of vulnerability to unexpected power loss or
crash. The old entry for a key is maintained until the new entry for that key
is written and verified.

Each KV entry has a unique transaction ID that is incremented for each KVS
update transaction. When determining system state from flash-stored KV entries,
the valid entry with the highest transaction ID is considered to be the
"current" entry of the key. All stored entries of the same key with lower
transaction IDs are considered old or "stale".

Updates/rewrites of a key that has been previously stored is done as a new KV
entry with an updated transaction ID and the new value for the key. The
internal state of the KVS is updated to reflect the new entry. The
previously stored KV entries for that key are not modified or removed from
flash storage, until garbage collection reclaims the "stale" entries.

`Garbage collection`_ is done by copying any currently valid KV entries in the
sector to be garbage collected to a different sector and then erasing the
sector.

Flash sectors
=============
Each flash sector is written sequentially in an append-only manner, with each
following entry write being at a higher address than all of the previous entry
writes to that sector since erase. Once information (header, metadata, data,
etc.) is written to flash, that information is not modified or cleared until a
full sector erase occurs as part of garbage collection.

Individual KV entries are contained within a single flash sector; they do
not cross sector boundaries. Flash sectors can contain as many KV entries as
fit in the sector.

Sectors are the minimum erase size for both :ref:`module-pw_kvs-design-memory`
and :ref:`module-pw_kvs-design-partitions`. Partitions may have a different
logical sector size than the memory they are part of. Partition logical sectors
may be smaller due to partition overhead (encryption, wear tracking, etc) or
larger due to combining raw sectors into larger logical sectors.

.. _module-pw_kvs-design-layers:

Storage layers
==============
The flash storage used by the KVS is comprised of two layers, flash memory
and flash partitions.

.. _module-pw_kvs-design-memory:

Flash memory
------------
``pw::kvs::FlashMemory`` is the lower storage layer that manages the raw
read/write/erase of the flash memory device. It is an abstract base class that
needs a concrete implementation before it can be used.

``pw::kvs::FakeFlashMemory`` is a variant that uses RAM rather than flash as
the storage media. This is helpful for reducing physical flash wear during unit
tests and development.

.. _module-pw_kvs-design-partitions:

Flash partitions
----------------
``pw::kvs::FlashPartition`` is a subset of a ``pw::kvs::FlashMemory``. Flash
memory may have one or multiple partition instances that represent different
parts of the memory, such as partitions for KVS, OTA, snapshots/crashlogs, etc.
Each partition has its own separate logical address space starting from zero to
``size`` bytes of the partition. Partition logical addresses do not always map
directly to memory addresses due to partition encryption, sector headers, etc.

Partitions support access via ``pw::kvs::NonSeekableWriter`` and
``pw::kvs::SeekableReader``. The reader defaults to the full size of the
partition but can optionally be limited to a smaller range.

``pw::kvs::FlashPartition`` is a concrete class that can be used directly. It
has several derived variants available, such as
``pw::kvs::FlashPartitionWithStats`` and
``pw::kvs::FlashPartitionWithLogicalSectors``.

.. _module-pw_kvs-design-alignment:

Alignment
=========
Writes to flash must have a start address that is a multiple of the flash
write alignment. Write size must also be a multiple of flash write alignment.
Write alignment varies by flash device and partition type. Reads from flash do
not have any address or size alignment requirement; reads always have a
minimum alignment of 1.

:ref:`module-pw_kvs-design-partitions` may have a different alignment than the
:ref:`module-pw_kvs-design-memory` they are part of, so long as the partition's
alignment is a multiple of the alignment for the memory.

.. _module-pw_kvs-design-allocation:

Allocation
----------
The KVS requires more storage space than the size of the key-value data
stored. This is due to the always-free sector required for garbage collection
and the "write and garbage collect later" approach it uses.

The KVS works poorly when stored data takes up more than 75% of the
available storage. It works best when stored data is less than 50%.
Applications that need to do garbage collection at scheduled times or that
write very heavily can benefit from additional flash store space.

The flash storage used by the KVS is multiplied by the amount of
:ref:`module-pw_kvs-design-redundancy` used. A redundancy of 2 will use twice
the storage, for example.

.. _module-pw_kvs-design-redundancy:

Redundancy
==========
The KVS supports storing redundant copies of KV entries. For a given redundancy
level (N), N total copies of each KV entry are stored. Redundant copies are
always stored in different sectors. This protects against corruption or even
full sector loss in N-1 sectors without data loss.

Redundancy increases flash usage proportional to the redundancy level. The RAM
usage for KVS internal state has a small increase with redundancy.

.. _module-pw_kvs-design-garbage:

Garbage collection
==================
Storage space occupied by stale :ref:`module-pw_kvs-design-entries` is
reclaimed and made available for reuse through a garbage collection process.
The base garbage collection operation is done to reclaim one sector at a time.

The KVS always keeps at least one sector free at all times to ensure the
ability to garbage collect. This free sector is used to copy valid entries from
the sector to be garbage collected before erasing the sector to be garbage
collected. The always-free sector is rotated as part of the KVS
:ref:`wear leveling <module-pw_kvs-design-wear>`.

Garbage collection can be performed manually, by invoking the methods below,
or it can be configured to happen automatically.

* :cpp:func:`pw::kvs::KeyValueStore::HeavyMaintenance()`
* :cpp:func:`pw::kvs::KeyValueStore::FullMaintenance()`
* :cpp:func:`pw::kvs::KeyValueStore::PartialMaintenance()`

.. _module-pw_kvs-design-wear:

Wear leveling (flash wear management)
=====================================
Wear leveling is accomplished by cycling selection of the next sector to write
to. This cycling spreads flash wear across all free sectors so that no one
sector is prematurely worn out.

The wear leveling decision-making process follows these guidelines:

* Location of new writes/rewrites of KV entries will prefer sectors already
  in-use (partially filled), with new (blank) sectors used when no in-use
  sectors have large enough available space for the new write.
* New (blank) sectors selected cycle sequentially between available free
  sectors.
* The wear leveling system searches for the first available sector, starting
  from the current write sector + 1 and wraps around to start at the end of a
  partition. This spreads the erase/write cycles for heavily written/rewritten
  KV entries across all free sectors, reducing wear on any single sector.
* Erase count is not considered in the wear leveling decision-making process.
* Sectors with already written KV entries that are not modified will remain in
  the original sector and not participate in wear-leveling, so long as the
  KV entries in the sector remain unchanged.

.. _module-pw_kvs-size:

------------------
Code size analysis
------------------
The following size report details the memory usage of ``KeyValueStore`` and
``FlashPartition``.

.. include:: kvs_size
