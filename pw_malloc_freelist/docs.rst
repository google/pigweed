.. _module-pw_malloc_freelist:

------------------
pw_malloc_freelist
------------------
.. pigweed-module::
   :name: pw_malloc_freelist

This module is deprecated. ``pw::allocator::FreeListHeap`` has been replaced by
``pw::allocator::BucketBlockAllocator``, which uses the same allocation
algorithm and implements the ``pw::Allocator`` API.

This module is now an alias to ``pw_malloc:bucket_block_allocator`` to
facilitate backwards-compatibility.
