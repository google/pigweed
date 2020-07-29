.. _chapter-pw-allocator:

.. default-domain:: cpp

-----------
pw_alloctor
-----------

This module provides various building blocks
for a dynamic allocator. This is composed of the following parts:

 - ``block``: An implementation of a linked list of memory blocks, supporting
   splitting and merging of blocks.
 - ``freelist``: A freelist, suitable for fast lookups of available memory
   chunks (i.e. ``block`` s).

Heap Integrity Check
====================
The ``Block`` class provides two sanity check functions:

  - ``bool Block::IsValid()``: Returns ``true`` is the given block is valid
    and ``false`` otherwise.
  - ``void Block::CrashIfInvalid()``: Crash the program and output the reason
    why sanity check fails using ``PW_DCHECK``.

Heap Poisoning
==============

By default, this module disables heap poisoning since it requires extra space.
User can enable heap poisoning by enabling the ``pw_allocator_POISON_HEAP``
build arg.

.. code:: sh

  $ gn args out
  # Modify and save the args file to use heap poison.
  pw_allocator_POISON_HEAP = true

When heap poisoning is enabled, ``pw_allocator`` will add ``sizeof(void*)``
bytes before and after the usable space of each ``Block``, and paint the space
with a hard-coded randomized pattern. During each sanity check, ``pw_allocator``
will check if the painted space still remains the pattern, and return ``false``
if the pattern is damaged.

Note, this module, and its documentation, is currently incomplete and
experimental.
