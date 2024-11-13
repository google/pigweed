.. _module-pw_elf:

.. cpp:namespace-push:: pw::elf

======
pw_elf
======
.. pigweed-module::
   :name: pw_elf

``pw_elf`` provides support for interact with Executable and Linkable Format
(ELF) files.

.. note::

   This module is currently very limited, primarily supporting other Pigweed
   modules. Additional functionality (e.g. iterating sections, segments) may be
   added in the future.

------
Guides
------

Read an ELF section into a buffer
=================================

.. literalinclude:: examples/reader.cc
   :language: cpp
   :linenos:
   :lines: 15-

-------------
API reference
-------------
.. doxygenclass:: pw::elf::ElfReader
   :members:
