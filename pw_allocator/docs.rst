.. _module-pw_allocator:

============
pw_allocator
============
.. pigweed-module::
   :name: pw_allocator
   :tagline: Flexible, safe, and measurable memory allocation
   :status: unstable
   :languages: C++17
   :code-size-impact: 400 to 1800 bytes

- **Flexible**: Simple interface makes it easy to inject specific behaviors.
- **Safe**: Can detect memory corruption, e.g overflows and use-after-free.
- **Measurable**: Pick what allocations you want to track and measure.

.. code-block:: cpp

   MyObject* Create(pw::allocator::Allocator& allocator) {
      void* buf = allocator.Allocate(pw::allocator::Layout::Of<MyObject>());
      return buf != nullptr ? new (buf) MyObject() : nullptr;
   }

   // Any of these allocators can be passed to the routine above.
   WithBuffer<LastFitBlockAllocator<uint32_t>, 0x1000> block_allocator;
   LibCAllocator libc_allocator;
   TrackingAllocator tracker(libc_allocator);
   SynchronizedAllocator synced(*block_allocator);


Dynamically allocate without giving up control! Pigweed's allocators let you
easily combine allocator features for your needs, without extra code size or
performance penalties for those you don't. Complex projects in particular can
benefit from dynamic allocation through simplified code, improved memory usage,
increased shared memory, and reduced large reservations.

**Want to allocate objects from specific memory like SRAM or PSRAM?**

Use `dependency injection`_! Write your code to take
:ref:`module-pw_allocator-api-allocator` parameters, and you can quickly and
easily change where memory comes from or what additional features are provided
simply by changing what allocator is passed.

.. code-block:: cpp

   // Set up allocators that allocate from SRAM or PSRAM memory.
   PW_PLACE_IN_SECTION(".sram")
   alignas(SmallFastObj) std::array<std::byte, 0x4000> sram_buffer;
   pw::allocator::FirstFitBlockAllocator<uint16_t> sram_allocator(sram_buffer);

   PW_PLACE_IN_SECTION(".psram")
   std::array<std::byte, 0x40000> psram_buffer;
   pw::allocator::WorstFitBlockAllocator<uint32_t>
     psram_allocator(psram_buffer);

   // Allocate objects from SRAM and PSRAM. Except for the name, the call sites
   // are agnostic about the kind of memory used.
   Layout small_fast_layout = pw::allocator::Layout::Of<SmallFastObj>();
   void* small_fast_buf = sram_allocator.Allocate(small_fast_layout);
   SmallFastObj* small_fast_obj = new (small_fast_buf) SmallFastObj();

   Layout big_slow_layout = pw::allocator::Layout::Of<BigSlowObj>();
   void* big_slow_buf = sram_allocator.Allocate(big_slow_layout);
   BigSlowObj* big_slow_obj = new (big_slow_buf) BigSlowObj();

**Worried about forgetting to deallocate?**

Use a smart pointer!

.. code-block:: cpp

   pw::allocator::UniquePtr<MyObject> ptr = allocator.MakeUnique<MyObject>();

**Want to know how much memory has been allocated?**

Pick the metrics you're interested in and track them with a
:cpp:type:`pw::allocator::TrackingAllocator`:

.. code-block:: cpp

   struct MyMetrics {
      PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
      PW_ALLOCATOR_METRICS_ENABLE(peak_allocated_bytes);
   };

   pw::allocator::TrackingAllocator<MyMetrics> tracker(allocator);

**Need to share the allocator with another thread or an interrupt handler?**

Use a :cpp:type:`pw::allocator::SynchronizedAllocator` with the lock of your
choice:

.. code-block:: cpp

   pw::sync::InterruptSpinLock isl;
   pw::allocator::SynchronizedAllocator<pw::sync::InterruptSpinLock>
      synchronized(tracker, isl);

.. tip:: Check out the :ref:`module-pw_allocator-guides` for even more code
   samples!

--------------------
Is it right for you?
--------------------
.. rst-class:: key-text

Does your project need to use memory...

- Without knowing exactly how much ahead of time?
- From different backing storage, e.g. SRAM vs. PSRAM?
- Across different tasks using a shared pool?
- In multiple places, and you need to know how much each place is using?

If you answered "yes" to any of these questions, ``pw_allocator`` may be able to
help! This module is designed to faciliate dynamic allocation for embedded
projects that are sufficiently complex to make static allocation infeasible.

Smaller projects may be able to enumerate their objects and preallocate any
storage they may need on device when running, and may be subject to extremely
tight memory constraints. In these cases, ``pw_allocator`` may add more costs in
terms of code size, overhead, and performance than it provides benefits in terms
of code simplicity and memory sharing.

At the other extreme, larger projects may be built on platforms with rich
operating system capabilities. On these platforms, the system and language
runtime may already provide dynamic allocation and memory may be less
constrained. In these cases, ``pw_allocator`` may not provide the same
capabilities as the platform.

Between these two is a range of complex projects on RTOSes and other platforms.
These projects may benefit from using the
:ref:`module-pw_allocator-api-allocator` interface and its implementations to
manage memory.

.. toctree::
   :hidden:
   :maxdepth: 1

   guide
   api
   design
   code_size

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Guides
      :link: module-pw_allocator-guides
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Integrate pw_allocator into your project and learn common use cases

   .. grid-item-card:: :octicon:`code-square` API reference
      :link: module-pw_allocator-api
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Detailed description of the pw_allocator's API

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Design & roadmap
      :link: module-pw_allocator-design
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Learn why pw_allocator is designed the way it is, and upcoming plans

   .. grid-item-card:: :octicon:`code-square` Code size analysis
      :link: module-pw_allocator-size-reports
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Understand pw_allocator's code footprint and savings potential

.. _dependency injection: https://en.wikipedia.org/wiki/Dependency_injection
