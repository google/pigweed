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

Note, this module, and its documentation, is currently incomplete and
experimental.

