.. _seed-0110:

==================================
0110: Memory Allocation Interfaces
==================================
.. seed::
   :number: 110
   :name: Memory Allocation Interfaces
   :status: Accepted
   :proposal_date: 2023-09-06
   :cl: 168772
   :authors: Alexei Frolov
   :facilitator: Taylor Cramer

-------
Summary
-------
With dynamic memory allocation becoming more common in embedded devices, Pigweed
should provide standardized interfaces for working with different memory
allocators, giving both upstream and downstream developers the flexibility to
move beyond manually sizing their modules' resource pools.

----------
Motivation
----------
Traditionally, most embedded firmware have laid out their systems' memory usage
statically, with every component's buffers and resources set at compile time.
As systems grow larger and more complex, the ability to dynamically allocate
memory has become more desirable by firmware authors.

Pigweed has made basic explorations into dynamic allocation in the past: the
``pw_allocator`` provides basic building blocks which projects can use to
assemble their own allocators. ``pw_allocator`` also provides a proof-of-concept
allocator (``FreeListHeap``) based off of these building blocks.

Since then, Pigweed has become a part of more complex projects and has
developed more advanced capabilities into its own modules. As the project has
progressed, several developers --- both on the core and customer teams --- have
noted areas where dynamic allocation would simplify code and improve memory
usage by enabling sharing and eliminating large reservations.

--------
Proposal
--------

Allocator Interfaces
====================
The core of the ``pw_allocator`` module will define abstract interfaces for
memory allocation. Several interfaces are provided with different allocator
properties, all of which inherit from a base generic ``Allocator``.

Core Allocators
---------------

Allocator
^^^^^^^^^

.. code-block:: c++

   class Allocator {
    public:
     class Layout {
      public:
       constexpr Layout(
           size_t size, std::align_val_t alignment = alignof(std::max_align_t))
           : size_(size), alignment_(alignment) {}

       // Creates a Layout for the given type.
       template <typename T>
       static constexpr Layout Of() {
         return Layout(sizeof(T), alignof(T));
       }

       size_t size() const { return size_; }
       size_t alignment() const { return alignment_; }

      private:
       size_t size_;
       size_t alignment_;
     };

     template <typename T, typename... Args>
     T* New(Args&&... args);

     template <typename T>
     void Delete(T* obj);

     template <typename T, typename... Args>
     std::optional<UniquePtr<T>> MakeUnique(Args&&... args);

     void* Allocate(Layout layout) {
       return DoAllocate(layout);
     }

     void Deallocate(void* ptr, Layout layout) {
       return DoDeallocate(layout);
     }

     bool Resize(void* ptr, Layout old_layout, size_t new_size) {
       if (ptr == nullptr) {
         return false;
       }
       return DoResize(ptr, old_layout, new_size);
     }

     void* Reallocate(void* ptr, Layout old_layout, size_t new_size) {
       return DoReallocate(void* ptr, Layout old_layout, size_t new_size);
     }

    protected:
     virtual void* DoAllocate(Layout layout) = 0;
     virtual void DoDeallocate(void* ptr, Layout layout) = 0;

     virtual bool DoResize(void* ptr, Layout old_layout, size_t new_size) {
       return false;
     }

     virtual void* DoReallocate(void* ptr, Layout old_layout, size_t new_size) {
       if (new_size == 0) {
         DoDeallocate(ptr, old_layout);
         return nullptr;
       }

       if (DoResize(ptr, old_layout, new_size)) {
         return ptr;
       }

       void* new_ptr = DoAllocate(new_layout);
       if (new_ptr == nullptr) {
         return nullptr;
       }

       if (ptr != nullptr && old_layout.size() != 0) {
         std::memcpy(new_ptr, ptr, std::min(old_layout.size(), new_size));
         DoDeallocate(ptr, old_layout);
       }

       return new_ptr;
     }
   };

``Allocator`` is the most generic and fundamental interface provided by the
module, representing any object capable of dynamic memory allocation.

The ``Allocator`` interface makes no guarantees about its implementation.
Consumers of the generic interface must not make any assumptions around
allocator behavior, thread safety, or performance.

**Layout**

Allocation parameters are passed to the allocator through a ``Layout`` object.
This object ensures that the values provided to the allocator are valid, as well
as providing some convenient helper functions for common allocation use cases,
such as allocating space for a specific type of object.

**Virtual functions**

Implementers of the allocator interface are responsible for providing the
following operations:

* ``DoAllocate`` (required): Obtains a block of memory from the allocator with a
  requested size and power-of-two alignment. Returns ``nullptr`` if the
  allocation cannot be performed.

  The size and alignment values in the provided layout are guaranteed to be
  valid.

  Memory returned from ``DoAllocate`` is uninitialized.

* ``DoDeallocate`` (required): Releases a block of memory back to the allocator.

  If ``ptr`` is ``nullptr``, does nothing.

  If ``ptr`` was not previously obtained from this allocator the behavior is
  undefined.

* ``DoResize`` (optional): Extends or shrinks a previously-allocated block of
  memory in place. If this operation cannot be performed, returns ``false``.

  ``ptr`` is guaranteed to be non-null. If ``ptr`` was not previously obtained
  from this allocator the behavior is undefined.

  If the allocated block is grown, the memory in the extended region is
  uninitialized.

* ``DoReallocate`` (optional): Extends or shrinks a previously-allocated block
  of memory, potentially copying its data to a different location. A default
  implementation is provided, which first attempts to call ``Resize``, falling
  back to allocating a new block and copying data if it fails.

  If ``ptr`` is ``nullptr``, behaves identically to ``Allocate(new_layout)``.

  If the new block cannot be allocated, returns ``nullptr``, leaving the
  original allocation intact.

  If ``new_layout.size == 0``, frees the old block and returns ``nullptr``.

  If the allocated block is grown, the memory in the extended region is
  uninitialized.

**Provided functions**

* ``New``: Allocates memory for an object from the allocator and constructs it.

* ``Delete``: Destructs and releases memory for a previously-allocated object.

* ``MakeUnique``: Allocates and constructs an object wrapped in a ``UniquePtr``
  which owns it and manages its release.

Allocator Utilities
===================
In addition to allocator interfaces, ``pw_allocator`` will provide utilities for
working with allocators in a system.

UniquePtr
---------
``pw::allocator::UniquePtr`` is a "smart pointer" analogous to
``std::unique_ptr``, designed to work with Pigweed allocators. It owns and
manages an allocated object, automatically deallocating its memory when it goes
out of scope.

Unlike ``std::unique_ptr``, Pigweed's ``UniquePtr`` cannot be manually
constructed from an existing non-null pointer; it must be done through the
``Allocator::MakeUnique`` API. This is required as the allocator associated with
the object allocation must be known in order to release it.

Usage reporting
---------------
``pw_allocator`` will not require any usage reporting as part of its core
interfaces to keep them minimal and reduce implementation burden.

However, ``pw_allocator`` encourages setting up reporting and will provide
utilities for doing so. Initially, this consists of a layered proxy allocator
which wraps another allocator implementation with basic usage reporting through
``pw_metric``.

.. code-block:: c++

   class AllocatorMetricProxy : public Allocator {
    public:
     constexpr explicit AllocatorMetricProxy(metric::Token token)
         : memusage_(token) {}

     // Sets the wrapped allocator.
     void Initialize(Allocator& allocator);

     // Exposed usage statistics.
     metric::Group& memusage() { return memusage_; }
     size_t used() const { return used_.value(); }
     size_t peak() const { return peak_.value(); }
     size_t count() const { return count_.value(); }

     // Implements the Allocator interface by forwarding through to the
     // sub-allocator provided to Initialize.

   };

Integration with C++ polymorphic memory resources
-------------------------------------------------
The C++ standard library has similar allocator interfaces to those proposed
defined as part of its PMR library. The reasons why Pigweed is not using these
directly are :ref:`described below <seed-0110-why-not-pmr>`; however, Pigweed
will provide a wrapper which exposes a Pigweed allocator through the PMR
``memory_resource`` interface. An example of how this wrapper might look is
presented here.

.. code-block:: c++

   template <typename Allocator>
   class MemoryResource : public std::pmr::memory_resource {
    public:
     template <typename... Args>
     MemoryResource(Args&&... args) : allocator_(std::forward<Args>(args)...) {}

    private:
     void* do_allocate(size_t bytes, size_t alignment) override {
       void* p = allocator_.Allocate(bytes, alignment);
       PW_ASSERT(p != nullptr);  // Cannot throw in Pigweed code.
       return p;
     }

     void do_deallocate(void* p, size_t bytes, size_t alignment) override {
       allocator_.Deallocate(p, bytes, alignment);
     }

     bool do_is_equal(const std::pmr::memory_resource&) override {
       // Pigweed allocators do not yet support the concept of equality; this
       // remains an open question for the future.
       return false;
     }

     Allocator allocator_;
   };

Future Considerations
=====================

Allocator traits
----------------
It can be useful for users to know additional details about a specific
implementation of an allocator to determine whether it is suitable for their
use case. For example, some allocators may have internal synchronization,
removing the need for external locking. Certain allocators may be suitable for
uses in specialized contexts such as interrupts.

To enable users to enforce these types of requirements, it would be useful to
provide a way for allocator implementations to define certain traits.
Originally, this proposal accommodated for this by defining derived allocator
interfaces which semantically enforced additional implementation contracts.
However, this approach could have led to an explosion of different allocator
types throughout the codebase for each permutation of traits. As such, it was
removed from the initial allocator plan for future reinvestigation.

Dynamic collections
-------------------
The ``pw_containers`` module defines several collections such as ``pw::Vector``.
These collections are modeled after STL equivalents, though being
embedded-friendly, they reserve a fixed maximum size for their elements.

With the addition of dynamic allocation to Pigweed, these containers will be
expanded to support the use of allocators. Unless absolutely necessary, upstream
containers should be designed to work on the base ``Allocator`` interface ---
not any of its derived classes --- to offer maximum flexibility to projects
using them.

.. code-block:: c++

   template <typename T>
   class DynamicVector {
     DynamicVector(Allocator& allocator);
   };

Per-allocation tagging
----------------------
Another interface which was originally proposed but shelved for the time being
allowed for the association of an integer tag with each specific call to
``Allocate``. This can be incredibly useful for debugging, but requires
allocator implementations to store additional information with each allocation.
This added complexity to allocators, so it was temporarily removed to focus on
refining the core allocator interface.

The proposed 32-bit integer tags happen to be the same as the tokens generated
from strings by the ``pw_tokenizer`` module. Combining the two could result in
the ability to precisely track the source of allocations in a project.

For example, ``pw_allocator`` could provide a macro which tokenizes a user
string to an allocator tag, automatically inserting additional metadata such as
the file and line number of the allocation.

.. code-block:: c++

   void GenerateAndProcessData(TaggedAllocator& allocator) {
     void* data = allocator->AllocatedTagged(
         Layout::Sized(kDataSize), PW_ALLOCATOR_TAG("my data buffer"));
     if (data == nullptr) {
       return;
     }

     GenerateData(data);
     ProcessData(data);

     allocator->Deallocate(data);
   }

Allocator implementations
-------------------------
Over time, Pigweed expects to implement a handful of different allocators
covering the interfaces proposed here. No specific new implementations are
suggested as part of this proposal. Pigweed's existing ``FreeListHeap``
allocator will be refactored to implement the ``Allocator`` interface.

---------------------
Problem Investigation
---------------------

Use cases and requirements
==========================

* **General-purpose memory allocation.** The target of ``pw_allocator`` is
  general-purpose dynamic memory usage by typical applications, rather than
  specialized types of memory allocation that may be required by lower-level
  code such as drivers.

* **Generic interfaces with minimal policy.** Every project has different
  resources and requirements, and particularly in constrained systems, memory
  management is often optimized for their specific use cases. Pigweed's core
  allocation interfaces should offer as broad of an implementation contract as
  possible and not bake in assumptions about how they will be run.

* **RTOS or bare metal usage.** While many systems make use of an RTOS which
  provides utilities such as threads and synchronization primitives, Pigweed
  also targets systems which run without one. As such, the core allocators
  should not be tied to any RTOS requirements, and accommodations should be made
  for different system contexts.

Out of scope
------------

* **Asynchronous allocation.** As this proposal is centered around simple
  general-purpose allocation, it does not consider asynchronous allocations.
  While these are important use cases, they are typically more specialized and
  therefore outside the scope of this proposal. Pigweed is considering some
  forms of asynchronous memory allocation, such as the proposal in the
  :ref:`Communication Buffers SEED <seed-0109>`.

* **Direct STL integration.** The C++ STL makes heavy use of dynamic memory and
  offers several ways for projects to plug in their own allocators. This SEED
  does not propose any direct Pigweed to STL-style allocator adapters, nor does
  it offer utilities for replacing the global ``new`` and ``delete`` operators.
  These are additions which may come in future changes.

  It is still possible to use Pigweed allocators with the STL in an indirect way
  by going through the PMR interface, which is discussed later.

* **Global Pigweed allocators.** Pigweed modules will not assume a global
  allocator instantiation. Any usage of allocators by modules should rely on
  dependency injection, leaving consumers with control over how they choose to
  manage their memory usage.

Alternative solutions
=====================

.. _seed-0110-why-not-pmr:

C++ polymorphic allocators
--------------------------
C++17 introduced the ``<memory_resource>`` header with support for polymorphic
memory resources (PMR), i.e. allocators. This library defines many allocator
interfaces similar to those in this proposal. Naturally, this raises the
question of whether Pigweed can use them directly, benefitting from the larger
C++ ecosystem.

The primary issue with PMR with regards to Pigweed is that the interfaces
require the use of C++ language features prohibited by Pigweed. The allocator
is expected to throw an exception in the case of failure, and equality
comparisons require RTTI. The team is not prepared to change or make exceptions
to this policy, prohibiting the direct usage of PMR.

Despite this, Pigweed's allocator interfaces have taken inspiration from the
design of PMR, incorporating many of its ideas. The core proposed ``Allocator``
interface is similar to ``std::pmr::memory_resource``, making it possible to
wrap Pigweed allocators with a PMR adapter for use with the C++ STL, albeit at
the cost of an extra layer of virtual indirection.

--------------
Open Questions
--------------
This SEED proposal is only a starting point for the improvement of the
``pw_allocator`` module, and Pigweed's memory management story in general.

There are several open questions around Pigweed allocators which the team
expects to answer in future SEEDs:

* Should generic interfaces for asynchronous allocations be provided, and how
  would they look?

* Reference counted allocations and "smart pointers": where do they fit in?

* The concept of allocator equality is essential to enable certain use cases,
  such as efficiently using dynamic containers with their own allocators.
  This proposal excludes APIs paralleling PMR's ``is_equal`` due to RTTI
  requirements. Could Pigweed allocators implement a watered-down version of an
  RTTI / type ID system to support this?

* How do allocators integrate with the monolithic ``pw_system`` as a starting
  point for projects?
