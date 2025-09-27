.. _module-pw_kvs-guides:

============================
Guides
============================

.. _module-pw_kvs-get-started:

----------
Quickstart
----------
This guide provides a walkthrough of how to set up and use ``pw_kvs``.

Build System Integration
========================
The first step is to add ``pw_kvs`` as a dependency in your build system.

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

KVS Setup Walkthrough
=====================
Setting up a ``pw_kvs::KeyValueStore`` involves three main stages:

1. :ref:`Implement the Hardware Interface <module-pw_kvs-guides-implement-hardware-interface>`:
   First, you must implement a C++ class that allows the KVS to communicate
   with your target's flash hardware.

2. :ref:`Configure and Instantiate the KVS <module-pw_kvs-guides-configure-and-instantiate-kvs>`:
   With the hardware interface in place, you can then create a
   ``KeyValueStore`` instance, defining the on-disk data format and memory
   buffers.

3. :ref:`Configure Garbage Collection <module-pw_kvs-guides-garbage-collection>`:
   Finally, you must decide how the KVS will perform garbage collection to
   reclaim space.

The following sections provide a detailed walkthrough of these stages.

.. _module-pw_kvs-guides-implement-hardware-interface:

Step 1: Implement the Hardware Interface
----------------------------------------
To use ``pw_kvs`` on a specific hardware platform, you must provide an
implementation of the ``pw::kvs::FlashMemory`` interface. This class provides a
hardware abstraction layer that the KVS uses to interact with the flash
storage.

The ``FlashMemory`` class defines the fundamental operations for interacting
with a flash device. You'll need to create a concrete class that inherits from
``pw::kvs::FlashMemory`` and implements its pure virtual functions (like
``Read``, ``Write``, and ``Erase``).

When creating your implementation, you must pass key hardware attributes to the
``FlashMemory`` base class constructor. You will need to read the datasheet for
your MCU or flash chip to determine these values. The most critical are:

- **Sector Size**: The smallest erasable unit of the flash memory, in bytes.
  All erases happen in multiples of this size.

- **Sector Count**: The total number of sectors in the flash device.

- **Alignment**: The minimum write size and address alignment for the flash
  hardware, in bytes. This dictates how the KVS packs data.

  - If your flash supports writing single bytes at any address, set alignment
    to ``1``.
  - If your flash has restrictions, such as only allowing a 4-byte word to be
    written once per erase cycle, your alignment must be ``4``. The KVS will
    then respect these boundaries, preventing invalid partial-word writes.

Once you have a ``FlashMemory`` implementation, you can create a
``FlashPartition``. A partition is a contiguous block of sectors within a
``FlashMemory`` that you dedicate to a specific purpose, such as a KVS.

.. code-block:: cpp

   #include "pw_kvs/flash_memory.h"

   // 1. A skeleton of a custom FlashMemory implementation.
   class MyFlashMemory : public pw::kvs::FlashMemory {
   public:
     MyFlashMemory()
         : pw::kvs::FlashMemory(kSectorSize, kSectorCount, kAlignment) {}

     // Implement the pure virtual functions from FlashMemory here...
     // Status Enable() override;
     // Status Disable() override;
     // bool IsEnabled() const override;
     // Status Erase(Address address, size_t num_sectors) override;
     // StatusWithSize Read(Address address, pw::span<std::byte> output) override;
     // StatusWithSize Write(Address address,
     //                      pw::span<const std::byte> data) override;

   private:
     static constexpr size_t kSectorSize = 4096;
     static constexpr size_t kSectorCount = 4;
     static constexpr size_t kAlignment = 4;
   };

   // 2. An instance of your FlashMemory.
   MyFlashMemory my_flash;

   // 3. A partition that uses the first 2 sectors of the flash.
   pw::kvs::FlashPartition partition(&my_flash, 0, 2);


.. _module-pw_kvs-guides-configure-and-instantiate-kvs:

Step 2: Configure and Instantiate the KVS
-----------------------------------------
After implementing the ``FlashMemory`` and creating a ``FlashPartition``, you
can create your ``KeyValueStore`` instance. This requires two final pieces of
configuration:

- **Entry Format**: The ``pw::kvs::EntryFormat`` struct specifies the magic
  value and checksum algorithm for KVS entries. For a detailed breakdown of the
  on-disk format, see :ref:`module-pw_kvs-disk-format-entry-structure`. The
  magic value is a unique identifier for your KVS, and the checksum is used to
  verify data integrity.

- **KVS Buffers**: The ``pw::kvs::KeyValueStoreBuffer`` template class requires
  you to specify the maximum number of entries and sectors the KVS can manage.
  This allocates the necessary memory for the KVS to operate.

Here is an example of how to create a ``KeyValueStore`` instance:

.. code-block:: cpp

   #include "my_flash_memory.h"  // Your FlashMemory implementation
   #include "pw_kvs/crc16_checksum.h"
   #include "pw_kvs/key_value_store.h"

   // Assumes `partition` from the previous step is available.

   pw::kvs::ChecksumCrc16 checksum;
   static constexpr pw::kvs::EntryFormat kvs_format = {
       .magic = 0xd253a8a9,
       .checksum = &checksum
   };

   constexpr size_t kMaxEntries = 64;
   constexpr size_t kMaxSectors = 2; // Must match the partition's sector count

   pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(
       &partition, kvs_format);

   kvs.Init();


.. _module-pw_kvs-guides-garbage-collection:

Step 3: Configure Garbage Collection
------------------------------------
The KVS requires periodic garbage collection (GC) to reclaim space from stale
or deleted entries. You must decide whether this will be triggered
automatically by the KVS or manually by your application.

Automatic Garbage Collection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
For most use cases, automatic GC is recommended. The KVS will automatically
run a GC cycle during a ``Put()`` operation if it cannot find enough space for
the new data. This is configured via the ``gc_on_write`` option passed to the
``KeyValueStore`` constructor.

.. code-block:: cpp

   pw::kvs::Options options;
   options.gc_on_write = pw::kvs::GargbageCollectOnWrite::kAsManySectorsNeeded;

   pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(
       &partition, kvs_format, options);

Available automatic GC options:

- ``kAsManySectorsNeeded`` (Default): The KVS will garbage collect as many
  sectors as needed to make space for the write.
- ``kOneSector``: The KVS will garbage collect at most one sector. If that is
  not enough to create space, the write will fail.
- ``kDisabled``: Disables automatic GC. See the manual section below.

Manual Garbage Collection
^^^^^^^^^^^^^^^^^^^^^^^^^
If your application requires fine-grained control over potentially long-running
flash operations, you can trigger GC manually. To do this, you must first
disable automatic GC:

.. code-block:: cpp

   pw::kvs::Options options;
   options.gc_on_write = pw::kvs::GargbageCollectOnWrite::kDisabled;

   pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(
       &partition, kvs_format, options);

Then, at appropriate times in your application's logic, you can call one of the
maintenance functions:

- ``kvs.PartialMaintenance()``: Performs GC on a single sector. This is useful
  for incrementally cleaning up the KVS over time.
- ``kvs.FullMaintenance()``: Performs a comprehensive GC of all sectors.


.. _module-pw_kvs-guides-advanced-topics:

---------------
Advanced Topics
---------------

.. _module-pw_kvs-guides-updating-kvs-configuration:

Updating KVS Configuration Over Time
====================================
A key consideration for long-lived products is how to handle firmware updates
that might need to change the KVS configuration. ``pw_kvs`` is designed to be
flexible, allowing for several types of changes to its size and layout.

Here are the general guidelines for what can be safely modified in a firmware
update.

Flash Partition and Sector Count
--------------------------------
The flash partition used by the KVS can be resized or even moved to a different
location in flash.

- **Increasing Sectors**: The number of sectors can be safely increased. The
  new flash partition can grow forwards, backwards, or be in a completely
  different location, as long as it includes all non-erased sectors from the
  old KVS instance.
- **Decreasing Sectors**: The number of sectors can be decreased, provided the
  new, smaller partition still contains all sectors that have valid KVS data.
- **Sector Size**: The logical sector size **must remain the same** across
  firmware updates. Changing the sector size will prevent the KVS from correctly
  interpreting the existing data.

Maximum Entry Count
-------------------
The maximum number of key-value entries the KVS can hold can be adjusted.

- **Increasing Entries**: The maximum entry count can be safely increased at any
  time. This simply allocates more RAM for tracking entries and doesn't affect
  the on-disk format.
- **Decreasing Entries**: The maximum entry count can be decreased, but the new
  limit must be greater than or equal to the number of entries currently stored
  in the KVS.

Redundancy
----------
The number of redundant copies for each entry can be changed.

- **Changing Redundancy Level**: The redundancy level can be safely increased or
  decreased between firmware updates. When the KVS is next initialized with the
  new redundancy level, it will detect the mismatch. During the next
  maintenance cycle (e.g., a call to ``PartialMaintenance()`` or
  ``FullMaintenance()``), the KVS will automatically write new redundant copies
  or ignore extra ones to match the new configuration.

Entry Format
------------
The ``EntryFormat`` defines the magic value and checksum algorithm for entries.

- **Adding New Formats**: To support backward compatibility, you can provide a
  list of ``EntryFormat`` structs to the ``KeyValueStore`` constructor. The KVS
  will be able to read entries matching any of the provided formats. The first
  format in the list is considered the "primary" format and will be used for all
  new entries written to the KVS.
- **Changing Existing Formats**: An existing ``EntryFormat`` (magic or checksum)
  **must not be changed**. Doing so would cause the KVS to fail to read existing
  entries, treating them as corrupt data.
