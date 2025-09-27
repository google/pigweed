.. _module-pw_kvs:

======
pw_kvs
======
.. pigweed-module::
   :name: pw_kvs

``pw_kvs`` is a flash-backed, persistent key-value storage system with
integrated wear leveling and redundancy, targeting NOR flash devices where a
full filesystem is not needed. Key properties include:

- **Wear Leveling**: Spreads writes across flash sectors to extend the life of
  the storage medium.
- **Corruption Resilience**: Optionally stores multiple copies of each key-value
  entry, spread over flash sectors. Corrupted entries are detected on read, and
  the KVS can fall back to a previous version of the data.
- **Flexibility**: Supports configuration changes after a product has shipped.
  or example, the total size of the KVS and the level of redundancy can be
  increased in a firmware update.
- **Simple On-Disk Format**: Log-structured database where entries are appended
  sequentially. This design has a minimal set of invariants, which simplifies
  implementation and improves robustness against unexpected power loss.

.. toctree::
   :hidden:
   :maxdepth: 1

   guides
   disk_format
   code_size

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Get Started & Guides
      :link: module-pw_kvs-guides
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Integrate and use ``pw_kvs`` in your project.

   .. grid-item-card:: :octicon:`code-square` API Reference
      :link: ../api/cc/group__pw__kvs.html
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      Detailed description of the ``pw_kvs`` classes and methods.

.. grid:: 2

   .. grid-item-card:: :octicon:`server` On-Disk Format & Design
      :link: module-pw_kvs-disk-format
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Understand how ``pw_kvs`` stores data on flash.

   .. grid-item-card:: :octicon:`graph` Code Size Analysis
      :link: module-pw_kvs-code-size
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Understand ``pw_kvs``'s code footprint.
