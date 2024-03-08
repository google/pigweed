.. _module-pw_allocator-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_allocator
   :tagline: "pw_allocator: Flexible, safe, and measurable memory allocation"

.. _module-pw_allocator-get-started:

-----------
Get started
-----------
.. tab-set::

   .. tab-item:: Bazel

      .. TODO: b/328076428 - Move to compiled example and inline.

      Add ``@pigweed//pw_allocator`` to the ``deps`` list in your Bazel target:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_allocator",
             # ...
           ]
         }

      For a narrower dependency, depend on subtargets, e.g.
      ``@pigweed//pw_allocator:tracking_allocator``.

      This assumes ``@pigweed`` is the name you pulled Pigweed into your Bazel
      ``WORKSPACE`` as.

      This assumes that your Bazel ``WORKSPACE`` has a `repository`_ named
      ``@pigweed`` that points to the upstream Pigweed repository.

   .. tab-item:: GN

      .. TODO: b/328076428 - Move to compiled example and inline.

      Add ``dir_pw_allocator`` to the ``deps`` list in your build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             dir_pw_allocator,
             # ...
           ]
         }

      For a narrower dependency, depend on subtargets, e.g.
      ``"$dir_pw_allocator:tracking_allocator"``.

      If the build target is a dependency of other targets and includes
      allocators in its public interface, e.g. its header file, use
      ``public_deps`` instead of ``deps``.

   .. tab-item:: CMake

      .. TODO: b/328076428 - Move to compiled example and inline.

      Add ``pw_allocator`` to your ``pw_add_library`` or similar CMake target:

      .. code-block::

         pw_add_library(my_library STATIC
           HEADERS
             ...
           PRIVATE_DEPS
             # ...
             pw_allocator
             # ...
         )

      For a narrower dependency, depend on subtargets, e.g.
      ``pw_allocator.tracking_allocator``, etc.

      If the build target is a dependency of other targets and includes
      allocators in its public interface, e.g. its header file, use
      ``PUBLIC_DEPS`` instead of ``PRIVATE_DEPS``.

-----------------
Inject allocators
-----------------
Routines that need to allocate memory dynamically should do so using the generic
:ref:`module-pw_allocator-api-allocator` interface. By using dependency
injection, memory users can be kept decoupled from the details of how memory is
provided. This yields the most flexibility for modifying or replacing the
specific allocators.

Use the ``Allocator`` interface
===============================
Write or refactor your memory-using routines to take
a pointer or reference to such an object and use the ``Allocate``,
``Deallocate``, ``Reallocate``, and ``Resize`` methods to manage memory. For
example:

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   class HeapBuffer final {
    public:
     HeapBuffer(Allocator& allocator) : allocator_(allocator) {}
     ~HeapBuffer() {
       allocator_.Reallocate(buffer_.data(), Layout(buffer_.size(), 1));
     }

     const std::byte* data() const { return buffer_.data(); }
     size_t size() const { return buffer_.size(); }

     Status CopyFrom(ByteSpan bytes) {
       void* new_ptr = allocator_.Reallocate(
         buffer_.data(),
         Layout(buffer_.size(), 1),
         bytes.size());
       if (new_ptr == nullptr) {
         return Status::ResourceExhausted;
       }
       buffer_ = ByteSpan(reinterpret_cast<std::byte*>(new_ptr), bytes.size());
       return OkStatus();
     }

    private:
     Allocator allocator_
     ByteSpan buffer_;
   };

To invoke methods or objects that inject allocators now requires an allocator to
have been constructed. The exact location where allocators should be
instantiated will vary from project to project, but it's likely to be early in a
program's lifecycle and in a device-specific location of the source tree.

For initial testing on :ref:`target-host`, a simple allocator such as
:ref:`module-pw_allocator-api-libc_allocator` can be used. This allocator is
trivally constructed and simply wraps ``malloc`` and ``free``.

Use UniquePtr<T>
================
Where possible, using `RAII`_ is a recommended approach for making memory
management easier and less error-prone.
:ref:`module-pw_allocator-api-unique_ptr` is a smart pointer that makes
allocating and deallocating memory more transparent:

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   class MyObjectFactory {
    public:
     MyObjectFactory(Allocator& allocator) : allocator_(allocator) {}
     UniquePtr<MyObject> MakeMyObject(ByteSpan foo, Vector<uint32_t> bar) {
       return allocator_.MakeUniquePtr<MyObject>(foo, std::move(bar));
     }
    private:
     Allocator& allocator_;
   };

Determine an allocation's Layout
================================
Several of the :ref:`module-pw_allocator-api-allocator` methods take a parameter
of the :ref:`module-pw_allocator-api-layout` type. This type combines the size
and alignment requirements of an allocation. It can be constructed directly, or
if allocating memory for a specific type, by using a templated static method:

.. code-block:: cpp

   Layout my_object_layout = Layout::Of<MyObject>();

As stated above, you should generally try to keep allocator implementation
details abstracted behind the :ref:`module-pw_allocator-api-allocator`
interface. One exception to this guidance is when integrating allocators into
existing code that assumes ``malloc`` and ``free`` semantics. Notably, ``free``
does not take any parameters beyond a pointer describing the memory to be freed.
To implement such a method using ``Deallocate``, you may need to violate the
abstraction and only use allocators that implement the optional ``GetLayout``
method:

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   static Allocator* allocator = nullptr;
   static constexpr size_t kAlignment = alignof(uintptr_t);

   void SetAllocator(Allocator& allocator_) { allocator = &allocator_; }

   void* pvPortMalloc(size_t size) {
     PW_CHECK_NOTNULL(allocator);
     return allocator->Allocator(Layout(size, kAlignment));
   }

   void pvPortFree(void* ptr) {
     PW_CHECK_NOTNULL(allocator);
     Result<Layout> old_layout = allocator->GetLayout();

     // Asserts if the allocator provided to ``SetAllocator`` does not implement
     // ``GetLayout``.
     allocator->Deallocate(ptr, old_layout.value());
   }

--------------------------
Choose the right allocator
--------------------------
Once one or more routines are using allocators, you can consider how best to
implement memory allocation for each particular scenario.

Concrete allocator implementations
==================================
This module provides several allocator implementations. The following is an
overview. Consult the :ref:`module-pw_allocator-api` for additional details.

- :ref:`module-pw_allocator-api-libc_allocator`: Uses ``malloc``, ``realloc``,
  and ``free``. This should only be used if the ``libc`` in use provides those
  functions.
- :ref:`module-pw_allocator-api-null_allocator`: Always fails. This may be
  useful if allocations should be disallowed under specific circumstances.
- :ref:`module-pw_allocator-api-block_allocator`: Tracks memory using
  :ref:`module-pw_allocator-api-block`. Derived types use specific strategies
  for how to choose a block to use to satisfy a request. See also
  :ref:`module-pw_allocator-design-block`. Derived types include:

  - :ref:`module-pw_allocator-api-first_fit_block_allocator`: Chooses the first
    block that's large enough to satisfy a request. This strategy is very fast,
    but may increase fragmentation.
  - :ref:`module-pw_allocator-api-last_fit_block_allocator`: Chooses the last
    block that's large enough to satisfy a request. This strategy is fast, and
    may fragment memory less than
    :ref:`module-pw_allocator-api-first_fit_block_allocator` when satisfying
    aligned memory requests.
  - :ref:`module-pw_allocator-api-best_fit_block_allocator`: Chooses the
    smallest block that's large enough to satisfy a request. This strategy
    maximizes the avilable space for large allocations, but may increase
    fragmentation and is slower.
  - :ref:`module-pw_allocator-api-worst_fit_block_allocator`: Chooses the
    largest block if it's large enough to satisfy a request. This strategy
    minimizes the amount of memory in unusably small blocks, but is slower.
  - :ref:`module-pw_allocator-api-dual_first_fit_block_allocator`: Acts like
    :ref:`module-pw_allocator-api-first_fit_block_allocator` or
    :ref:`module-pw_allocator-api-last_fit_block_allocator` depending on
    whether a request is larger or smaller, respectively, than a given
    threshold value. This strategy preserves the speed of the two other
    strategies, while fragmenting memory less by co-locating allocations of
    similar sizes.

.. TODO: b/328076428 - Add MonotonicAllocator.

.. TODO: b/328076428 - Add BuddyAllocator.

.. TODO: b/328076428 - Add SlabAllocator.

Forwarding allocator implementations
====================================
This module provides several "forwarding" allocators, as described in
:ref:`module-pw_allocator-design-forwarding`. The following is an overview.
Consult the :ref:`module-pw_allocator-api` for additional details.

- :ref:`module-pw_allocator-api-fallback_allocator`: Dispatches first to a
  primary allocator, and, if that fails, to a secondary allocator.
- :ref:`module-pw_allocator-api-synchronized_allocator`: Synchronizes access to
  another allocator, allowing it to be used by multiple threads.
- :ref:`module-pw_allocator-api-tracking_allocator`: Wraps another allocator and
  records its usage.

.. TODO: b/328076428 - Add MemoryResource.

Custom allocator implementations
================================
If none of the allocator implementations provided by this module meet your
needs, you can implement your allocator and pass it into any routine that uses
the generic interface.

:ref:`module-pw_allocator-api-allocator` uses an `NVI`_ pattern. A custom
allocator implementation must at a miniumum implement the ``DoAllocate`` and
``DoDeallocate`` methods.

There are also several optional methods you can provide:

- If an implementation of ``DoResize`` isn't provided, then ``Resize`` will
  always return false.
- If an implementation of ``DoReallocate`` isn't provided, then ``Reallocate``
  will try to ``Resize``, and, if unsuccessful, try to ``Allocate``, copy, and
  ``Deallocate``.
- If an implementation of ``DoGetLayout`` isn't provided, then ``GetLayout``
  will always return ``pw::Status::Unimplmented``.
- If an implementation of ``DoQuery`` isn't provided, then ``Query`` will
  always return ``pw::Status::Unimplmented``.

.. TODO: b/328076428 - Make Deallocate optional once traits supporting
   MonotonicAllocator are added.

--------------------
Measure memory usage
--------------------
You can observe how much memory is being used for a particular use case using a
:ref:`module-pw_allocator-api-tracking_allocator`.

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   static LibCAllocator my_allocator;
   static TrackingAllocator<AllMetrics> tracker(my_allocator);

   int main() {
      Run(tracker);
      tracker.Dump();
   }

Metric data can be retrieved according to the steps described in
:ref:`module-pw_metric-exporting`, or by using the ``Dump`` method of
:ref:`module-pw_metric-group`.

The ``AllMetrics`` type used in the example above enables the following metrics:

- **allocated_bytes**: The number of bytes currently allocated by this
  allocator.
- **peak_allocated_bytes**: The most bytes allocated by this allocator at any
  one time.
- **cumulative_allocated_bytes**: The total number of bytes that have been
  allocated by this allocator across its lifetime.
- **num_allocations**: The number of allocation requests this allocator has
  successfully completed.
- **num_deallocations**: The number of deallocation requests this allocator has
  successfully completed.
- **num_resizes**: The number of resize requests this allocator has successfully
  completed.
- **num_reallocations**: The number of reallocation requests this allocator has
  successfully completed.
- **num_failures**: The number of requests this allocator has failed to
  complete.

If you only want a subset of these metrics, you can implement your own metrics
struct. For example:

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   struct MyMetrics {
     PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
     PW_ALLOCATOR_METRICS_ENABLE(peak_allocated_bytes);
     PW_ALLOCATOR_METRICS_ENABLE(num_failures);
   };

If you have multiple routines that share an underlying allocator that you want
to track separately, you can create multiple tracking allocators that forward to
the same underlying allocator:

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   constexpr size_t kSramSize = 0x10000;
   PW_PLACE_IN_SECTION("sram") std::array<std::byte, kSramSize> sram_buffer;
   BestFitBlockAllocator<uint32_t> sram_allocator(sram_buffer);

   TrackingAllocator use_case1_tracker(sram_allocator);
   TrackingAllocator use_case2_tracker(sram_allocator);

Visualize the heap
==================
``pw_allocator`` supplies a Pigweed environment command ``pw heap-viewer`` to
help visualize the state of the heap at the end of a dump file. The heap is
represented by ASCII characters, where each character represents 4 bytes in the
heap.

.. image:: doc_resources/pw_allocator_heap_visualizer_demo.png

The heap visualizer can be launched from a shell using the Pigweed environment.

.. code-block:: sh

  $ pw heap-viewer --dump-file <directory of dump file> --heap-low-address
  <hex address of heap lower address> --heap-high-address <hex address of heap
  lower address> [options]

The required arguments are:

- ``--dump-file`` is the path of a file that contains ``malloc/free``
  information. Each line in the dump file represents a ``malloc/free`` call.
  ``malloc`` is represented as ``m <size> <memory address>`` and ``free`` is
  represented as ``f <memory address>``. For example, a dump file should look
  like:

  .. code-block:: sh

    m 20 0x20004450  # malloc 20 bytes, the pointer is 0x20004450
    m 8 0x2000447c   # malloc 8 bytes, the pointer is 0x2000447c
    f 0x2000447c     # free the pointer at 0x2000447c
    ...

  Any line not formatted as the above will be ignored.

- ``--heap-low-address`` is the start of the heap. For example:

  .. code-block:: sh

    --heap-low-address 0x20004440

- ``--heap-high-address`` is the end of the heap. For example:

  .. code-block:: sh

    --heap-high-address 0x20006040

Options include the following:

- ``--poison-enable``: If heap poisoning is enabled during the
  allocation or not. The value is ``False`` if the option is not specified and
  ``True`` otherwise.

- ``--pointer-size <integer of pointer size>``: The size of a pointer on the
  machine where ``malloc/free`` is called. The default value is ``4``.

------------------------
Detect memory corruption
------------------------
The :ref:`module-pw_allocator-design-block` class provides a few different
mechanisms to help detect memory corruptions when they happen. First, on every
deallocation it will check the integrity of the block header and assert if it
has been modified.

Additionally, you can enable poisoning to detect additional memory corruptions
such as use-after-frees. The ``Block`` type has a template parameter that
enables the ``Poison`` and ``CheckPoison`` methods. Allocators can use these
methods to "poison" blocks on deallocation with a set pattern, and later check
on allocation that the pattern is intact. If it's not, some routine has
modified unallocated memory.

The :ref:`module-pw_allocator-api-block_allocator` class has a
``kPoisonInterval`` template parameter to control how frequently blocks are
poisoned on deallocation. This allows projects to stochiastically sample
allocations for memory corruptions while mitigating the performance impact.

.. TODO: b/328076428 - Move to compiled example and inline.

.. code-block:: cpp

   // Poisons every 2048th deallocation.
   LastFitBlockAllocator<uint32_t, 2048> allocator;

----------------------
Test custom allocators
----------------------
If you create your own allocator implementation, it's strongly recommended that
you test it as well. If you're creating a forwarding allocator, you can use
:ref:`module-pw_allocator-api-allocator_for_test`. This simple allocator
provides its own backing storage and automatically frees any outstanding
allocations when it goes out of scope. It also tracks the most recent values
provided as parameters to the interface methods.

.. code-block:: cpp

   // Class being tested.
   class MyAllocator {
    public:
     MyAllocator(Allocator& allocator) : allocator_(allocator) {}
    private:
     void* DoAllocate(Layout layout) {
       // Perform some side effects here...
       return allocator_.Allocate(layout);
     }

     void DoDeallocate(void* ptr, Layout layout) {
       allocator_.Deallocate(ptr, layout);
     }

     Allocator& allocator_;
   };

   TEST(MyAllocatorTest, SomeBehaviorOnAllocate) {
     AllocatorForTest<256> allocator;
     MyAllocator my_allocator(allocator);
     auto result = my_allocator.MakeUnique<MyObject>();
     ASSERT_TRUE(result.has_value());
     EXPECT_EQ(allocator.allocate_size(), sizeof(MyObject));
     // Check some other side effects here...
   }

You can also extend the :ref:`module-pw_allocator-api-allocator_test_harness` to
perform pseudorandom sequences of allocations and deallocations, e.g. as part of
a performance test:

.. code-block:: cpp

   class MyAllocatorTestHarness : public AllocatorTestHarness {
    public:
     static constexpr size_t kCapacity = 0x4000;

     MyAllocatorTestHarness() : my_allocator_(allocator_) {}
     ~MyAllocatorTestHarness() override = default;

     Allocator* Init() override { return &my_allocator_; }

    private:
     AllocatorForTest<kCapacity> allocator_;
     MyAllocator my_allocator_;
   };

   void PerformAllocations(pw::perf_test::State& state, uint64_t seed) {
     static MyAllocatorTestHarness harness;
     random::XorShiftStarRng64 prng(seed);
     while(state.KeepRunning()) {
       harness.GenerateRequest(prng, MyAllocatorTestHarness::kCapacity);
     }
     harness.Reset();
   }

   PW_PERF_TEST(MyAllocatorPerfTest, PerformAllocations, 1);

Even better, you can easily add fuzz tests for your allocator. This module
uses the ``AllocatorTestHarness`` to integrate with :ref:`module-pw_fuzzer` and
provide :ref:`module-pw_allocator-api-fuzzing_support`.

.. code-block:: cpp

   void MyAllocatorNeverCrashes(const Vector<AllocatorRequest>& requests) {
     static MyAllocatorTestHarness harness;
     harness.HandleRequests(requests);
   }

   FUZZ_TEST(MyAllocator, MyAllocatorNeverCrashes)
     .WithDomains(ArbitraryAllocatorRequests<kMaxRequests, kMaxSize>());

-----------------------------
Measure custom allocator size
-----------------------------
If you create your own allocator implementation, you may wish to measure its
code size, similar to measurements in the module's own
:ref:`module-pw_allocator-size-reports`. You can use ``pw_bloat`` and
:ref:`module-pw_allocator-api-size_reporter` to create size reports as described
in :ref:`bloat-howto`.

For example, the C++ code for a size report binary might look like:

.. code-block:: cpp

   #include "pw_allocator/size_reporter.h"

   namespace pw::allocator {

   void Run() {
     pw::allocator::SizeReporter size_reporter;
     FirstFitBlockAllocator<uint16_t> allocator(size_reporter.buffer());
     MyAllocator my_allocator(allocator);
     size_reporter.MeasureAllocator<void>(&my_allocator);
   }

   }  // namespace pw::allocator

   int main() {
     pw::allocator::Run();
     return 0;
   }

The resulting binary could be compared with the binary produced from
pw_allocator/size_report/first_fit_block_allocator.cc to identify just the code
added in this case by ``MyAllocator``.

.. _NVI: https://en.wikipedia.org/wiki/Non-virtual_interface_pattern
.. _RAII: https://en.cppreference.com/w/cpp/language/raii
.. _repository: https://bazel.build/concepts/build-ref#repositories
