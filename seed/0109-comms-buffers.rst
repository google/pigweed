.. _seed-0109:

===========================
0109: Communication Buffers
===========================
.. seed::
   :number: 109
   :name: Communication Buffers
   :status: Accepted
   :proposal_date: 2023-08-28
   :cl: 168357
   :facilitator: Erik Gilling

-------
Summary
-------
This SEED proposes that Pigweed adopt a standard buffer type for network-style
communications. This buffer type will be used in the new sockets API
(see the recently-accepted `Communications SEED 107
<https://pigweed.dev/seed/0107-communications.html>`_ for more details), and
will be carried throughout the communications stack as-appropriate.

------------------------
Top-Level Usage Examples
------------------------

Sending a Proto Into a Socket
=============================
.. code-block:: cpp

   Allocator& msg_allocator = socket.Allocator();
   size_t size = EncodedProtoSize(some_proto);
   std::optional<pw::MultiBuf> buffer = msg_allocator.Allocate(size);
   if (!buffer) { return; }
   EncodeProto(some_proto, *buffer);
   socket.Send(std::move(*buffer));

``Socket`` s provide an allocator which should be used to create a
``pw::MultiBuf``. The data to be sent is then filled into this buffer before
being sent. ``Socket`` must accept ``pw::MultiBuf`` s which were not created
by their own ``Allocator``, but this may incur a performance penalty.

Zero-Copy Fragmentation
=======================

The example above has a hidden feature: zero-copy fragmentation! The socket
can provide a ``pw::MultiBuf`` which is divided up into separate MTU-sized
chunks, each of which has reserved space for headers and/or footers:

.. code-block:: cpp

   std::optional<pw::MultiBuf> Allocate(size_t size) {
     if (size == 0) { return pw::MultiBuf(); }
     size_t data_per_chunk = mtu_size_ - header_size_;
     size_t num_chunks = 1 + ((size - 1) / data_per_chunk);
     std::optional<pw::MultiBuf> buffer = pw::MultiBuf::WithCapacity(num_chunks);
     if (!buffer) { return std::nullopt; }
     for (size_t i = 0; i < num_chunks; i++) {
       // Note: we could allocate smaller than `mtu_size_` for the final
       // chunk. This is ommitted here for brevity.
       std::optional<pw::MultiBuf::Chunk> chunk = internal_alloc_.Allocate(mtu_size_);
       if (!chunk) { return std::nullopt; }
       // Reserve the header size by discarding the front of each buffer.
       chunk->DiscardFront(header_size_);
       buffer->Chunks()[i] = std::move(chunk);
     }
     return *buffer;
   }

This code reserves ``header_size_`` bytes at the beginning of each ``Chunk``.
When the caller writes into this memory and then passes it back to the socket,
these bytes can be claimed for the header using the ``ClaimPrefix`` function.

One usecase that seems to demand the ability to fragment like this is breaking
up ``SOCK_SEQPACKET`` messages which (at least on Unix / Linux) may be much larger
than the MTU size: up to ``SO_SNDBUF`` (see `this man page
<https://man7.org/linux/man-pages/man7/socket.7.html>`_).

Multiplexing Incoming Data
==========================

.. code-block:: cpp

   [[nodiscard]] bool SplitAndSend(pw::MultiBuf buffer) {
     std::optional<std::array<pw::MultiBuf, 2>> buffers =
       std::move(buffer).Split(split_index);
     if (!buffers) { return false; }
     socket_1_.Send(std::move(buffers->at(0)));
     socket_2_.Send(std::move(buffers->at(1)));
     return true;
   }

Incoming buffers can be split without copying, and the results can be forwarded
to multiple different ``Socket`` s, RPC services or clients.

----------
Motivation
----------
Today, a Pigweed communications stack typically involves a number of different
buffers.

``pw_rpc`` users, for example, frequently use direct-memory access (DMA) or
other system primitives to read data into a buffer, apply some link-layer
protocol such as HDLC which copies data into a second buffer, pass the resulting
data into pw_rpc which parses it into its own buffer. Multiple sets of buffers
are present on the output side as well. Between DMA in and DMA out, it's easy
for data to pass through six or more different buffers.

These independent buffer systems introduce both time and space overhead. Aside
from the additional CPU time required to move the data around, users need to
ensure that all of the different buffer pools involved along the way have enough
space reserved to contain the entire message. Where caching is present, moving
the memory between locations may create an additional delay by thrashing
between memory regions.

--------
Proposal
--------
``pw::buffers::MultiBuf`` is a handle to a buffer optimized for use within
Pigweed's communications stack. It provides efficient, low-overhead buffer
management, and serves as a standard type for passing data between drivers,
TCP/IP implementations, RPC 2.0, and transfer 2.0.

A single ``MultiBuf`` is a list of ``Chunk`` s of data. Each ``Chunk``
points to an exclusively-owned portion of a reference-counted allocation.
``MultiBuf`` s can be easily split, joined, prefixed, postfixed, or infixed
without needing to copy the underlying data.

The memory pointed to by ``Chunk`` s is typically allocated from a pool
provided by a ``Socket``. This allows the ``Socket`` to provide backpressure to
callers, and to ensure that memory is placed in DMA or shared memory regions
as-necessary.

In-Memory Layout
================

This diagram shows an example in-memory relationship between two buffers:
- ``Buffer1`` references one chunks from region A.
- ``Buffer2`` references two chunk from regions A and B.

.. mermaid::

   graph TB;
   Buffer1 --> Chunk1A
   Chunk1A -- "[0..64]" --> MemoryRegionA(Memory Region A)
   Chunk1A --> ChunkRegionTrackerA
   Buffer2 --> Chunk2A & Chunk2B
   Chunk2A --> ChunkRegionTrackerA
   Chunk2A -- "[64..128]" --> MemoryRegionA(Memory Region A)
   Chunk2B -- "[0..128]" --> MemoryRegionB
   Chunk2B --> ChunkRegionTrackerB

API
===

The primary API is as follows:

.. code-block:: cpp

   /// An object that manages a single allocated region which is referenced
   /// by one or more chunks.
   class ChunkRegionTracker {
    public:
     ChunkRegionTracker(ByteSpan);

     /// Creates the first ``Chunk`` referencing a whole region of memory.
     /// This must only be called once per ``ChunkRegionTracker``.
     Chunk ChunkForRegion();

    protected:
     pw::Mutex lock();

     /// Destroys the `ChunkRegionTracker`.
     ///
     /// Typical implementations will call `std::destroy_at(this)` and then
     /// free the memory associated with the tracker.
     virtual void Destroy();

     /// Defines the entire span of the region being managed. ``Chunk`` s
     /// referencing this tracker may not expand beyond this region
     /// (or into one another).
     ///
     /// This region must not change for the lifetime of this
     /// ``ChunkRegionTracker``.
     virtual ByteSpan region();

    private:
     /// Used to manage the internal linked-list of ``Chunk`` s that allows
     /// chunks to know whether or not they can expand to fill neighboring
     /// regions of memory.
     pw::Mutex lock_;
   };

   /// A handle to a contiguous refcounted slice of data.
   ///
   /// Note: this Chunk may acquire a ``pw::sync::Mutex`` during destruction, and
   /// so must not be destroyed within ISR context.
   class Chunk {
    public:
     Chunk();
     Chunk(ChunkRegionTracker&);
     Chunk(Chunk&& other) noexcept;
     Chunk& operator=(Chunk&& other);
     ~Chunk();
     std::byte* data();
     const std::byte* data() const;
     ByteSpan span();
     ConstByteSpan span() const;
     size_t size() const;

     std::byte& operator[](size_t index);
     std::byte operator[](size_t index) const;

     /// Decrements the reference count on the underlying chunk of data and empties
     /// this handle so that `span()` now returns an empty (zero-sized) span.
     ///
     /// Does not modify the underlying data, but may cause it to be
     /// released if this was the only remaining ``Chunk`` referring to it.
     /// This is equivalent to ``{ Chunk _unused = std::move(chunk_ref); }``
     void Release();

     /// Attempts to add `prefix_bytes` bytes to the front of this buffer by
     /// advancing its range backwards in memory. Returns `true` if the
     /// operation succeeded.
     ///
     /// This will only succeed if this `Chunk` points to a section of a chunk
     /// that has unreferenced bytes preceeding it. For example, a `Chunk`
     /// which has been shrunk using `DiscardFront` can then be re-expanded using
     /// `ClaimPrefix`.
     ///
     /// Note that this will fail if a preceding `Chunk` has claimed this
     /// memory using `ClaimSuffix`.
     [[nodiscard]] bool ClaimPrefix(size_t prefix_bytes);

     /// Attempts to add `suffix_bytes` bytes to the back of this buffer by
     /// advancing its range forwards in memory. Returns `true` if the
     /// operation succeeded.
     ///
     /// This will only succeed if this `Chunk` points to a section of a chunk
     /// that has unreferenced bytes following it. For example, a `Chunk`
     /// which has been shrunk using `Truncate` can then be re-expanded using
     /// `ClaimSuffix`.
     ///
     /// Note that this will fail if a following `Chunk` has claimed this
     /// memory using `ClaimPrefix`.
     [[nodiscard]] bool ClaimSuffix(size_t suffix_bytes);

     /// Shrinks this handle to refer to the data beginning at offset ``new_start``.
     ///
     /// Does not modify the underlying data.
     void DiscardFront(size_t new_start);

     /// Shrinks this handle to refer to data in the range ``begin..<end``.
     ///
     /// Does not modify the underlying data.
     void Slice(size_t begin, size_t end);

     /// Shrinks this handle to refer to only the first ``len`` bytes.
     ///
     /// Does not modify the underlying data.
     void Truncate(size_t len);

     /// Splits this chunk in two, with the second chunk starting at `split_point`.
     std::array<Chunk, 2> Split(size_t split_point) &&;
   };

   /// A handle to a sequence of potentially non-contiguous refcounted slices of
   /// data.
   class MultiBuf {
    public:
     struct Index {
       size_t chunk_index;
       size_t byte_index;
     };

     MultiBuf();

     /// Creates a ``MultiBuf`` pointing to a single, contiguous chunk of data.
     ///
     /// Returns ``std::nullopt`` if the ``ChunkList`` allocator is out of memory.
     static std::optional<MultiBuf> FromChunk(Chunk chunk);

     /// Splits the ``MultiBuf`` into two separate buffers at ``split_point``.
     ///
     /// Returns ``std::nullopt`` if the ``ChunkList`` allocator is out of memory.
     std::optional<std::array<MultiBuf, 2>> Split(Index split_point) &&;
     std::optional<std::array<MultiBuf, 2>> Split(size_t split_point) &&;

     /// Appends the contents of ``suffix`` to this ``MultiBuf`` without copying data.
     /// Returns ``false`` if the ``ChunkList`` allocator is out of memory.
     [[nodiscard]] bool Append(MultiBuf suffix);

     /// Discards the first elements of ``MultiBuf`` up to (but not including)
     /// ``new_start``.
     ///
     /// Returns ``false`` if the ``ChunkList`` allocator is out of memory.
     [[nodiscard]] bool DiscardFront(Index new_start);
     [[nodiscard]] bool DiscardFront(size_t new_start);

     /// Shifts and truncates this handle to refer to data in the range
     /// ``begin..<stop``.
     ///
     /// Does not modify the underlying data.
     ///
     /// Returns ``false`` if the ``ChunkList`` allocator is out of memory.
     [[nodiscard]] bool Slice(size_t begin, size_t end);

     /// Discards the tail of this ``MultiBuf`` starting with ``first_index_to_drop``.
     /// Returns ``false`` if the ``ChunkList`` allocator is out of memory.
     [[nodiscard]] bool Truncate(Index first_index_to_drop);
     /// Discards the tail of this ``MultiBuf`` so that only ``len`` elements remain.
     /// Returns ``false`` if the ``ChunkList`` allocator is out of memory.
     [[nodiscard]] bool Truncate(size_t len);

     /// Discards the elements beginning with ``cut_start`` and resuming at
     /// ``resume_point``.
     ///
     /// Returns ``false`` if the ``ChunkList`` allocator is out of memory.
     [[nodiscard]] bool DiscardSegment(Index cut_start, Index resume_point);

     /// Returns an iterable over the ``Chunk``s of memory within this ``MultiBuf``.
     auto Chunks();
     auto Chunks() const;

     /// Returns a ``BidirectionalIterator`` over the bytes in this ``MultiBuf``.
     ///
     /// Note that use of this iterator type may be less efficient than
     /// performing chunk-wise operations due to the noncontiguous nature of
     /// the iterator elements.
     auto begin();
     auto end();

     /// Counts the total number of ``Chunk`` s.
     ///
     /// This function is ``O(n)`` in the number of ``Chunk`` s.
     size_t CalculateNumChunks() const;

     /// Counts the total size in bytes of all ``Chunk`` s combined.
     ///
     /// This function is ``O(n)`` in the number of ``Chunk`` s.
     size_t CalculateTotalSize() const;

     /// Returns an ``Index`` which can be used to provide constant-time access to
     /// the element at ``position``.
     ///
     /// Any mutation of this ``MultiBuf`` (e.g. ``Append``, ``DiscardFront``,
     /// ``Slice``, or ``Truncate``) may invalidate this ``Index``.
     Index IndexAt(size_t position) const;
   };


   class MultiBufAllocationFuture {
    public:
     Poll<std::optional<Buffer>> Poll(Context&);
   };

   class MultiBufAllocationFuture {
    public:
     Poll<std::optional<MultiBuf::Chunk>> Poll(Context&);
   };

   class MultiBufAllocator {
    public:
     std::optional<MultiBuf> Allocate(size_t size);
     std::optional<MultiBuf> Allocate(size_t min_size, size_t desired_size);
     std::optional<MultiBuf::Chunk> AllocateContiguous(size_t size);
     std::optional<MultiBuf::Chunk> AllocateContiguous(size_t min_size, size_t desired_size);

     MultiBufAllocationFuture AllocateAsync(size_t size);
     MultiBufAllocationFuture AllocateAsync(size_t min_size, size_t desired_size);
     MultiBufChunkAllocationFuture AllocateContiguousAsync(size_t size);
     MultiBufChunkAllocationFuture AllocateContiguousAsync(size_t min_size, size_t desired_size);

    protected:
     virtual std::optional<MultiBuf> DoAllocate(size_t min_size, size_t desired_size);
     virtual std::optional<MultiBuf::Chunk> DoAllocateContiguous(size_t min_size, size_t desired_size);

     /// Invoked by the ``MultiBufAllocator`` to signal waiting futures that buffers of
     /// the provided sizes may be available for allocation.
     void AllocationAvailable(size_t size_available, size_t contiguous_size_available);
   };


The ``Chunk`` s in the prototype are stored in fallible dynamically-allocated
arrays, but they could be stored in vectors of a fixed maximum size. The ``Chunk`` s
cannot be stored as an intrusively-linked list because this would require per-``Chunk``
overhead in the underlying buffer, and there may be any number of ``Chunk`` s within
the same buffer.

Multiple ``Chunk`` s may not refer to the same memory, but they may refer to
non-overlapping sections of memory within the same region. When one ``Chunk``
within a region is deallocated, a neighboring chunk may expand to use its space.

--------------------
Vectorized Structure
--------------------
The most significant design choices made in this API is supporting vectorized
IO via a list of ``Chunk`` s. While this does carry an additional overhead, it
is strongly motivated by the desire to carry data throughout the communications
stack without needing a copy. By carrying a list of ``Chunk`` s, ``MultiBuf``
allows data to be prefixed, suffixed, infixed, or split without incurring the
overhead of a separate allocation and copy.

--------------------------------------------------------------------------
Managing Allocations with ``MultiBufAllocator`` and ``ChunkRegionTracker``
--------------------------------------------------------------------------
``pw::MultiBuf`` is not associated with a concrete allocator implementation.
Instead, it delegates the creation of buffers to implementations of
the ``MultiBufAllocator`` base class. This allows different allocator
implementations to vend out ``pw::MultiBuf`` s that are optimized for use with a
particular communications stack.

For example, a communications stack which runs off of shared memory or specific
DMA'able regions might choose to allocate memory out of those regions to allow
for zero-copy writes.

Additionally, custom allocator implementations can reserve headers, footers, or
even split a ``pw::MultiBuf`` into multiple chunks in order to provide zero-copy
fragmentation.

Deallocation of these regions is managed through the ``ChunkRegionTracker``.
When no more ``Chunk`` s associated with a ``ChunkRegionTracker`` exist,
the ``ChunkRegionTracker`` receives a ``Destroy`` call to release both the
region and the ``ChunkRegionTracker``.

The ``MultiBufAllocator`` can place the ``ChunkRegionTracker`` wherever it
wishes, including as a header or footer for the underlying region allocation.
This is not required, however, as memory in regions like shared or DMA'able
memory might be limited, in which case the ``ChunkRegionTracker`` can be
stored elsewhere.

-----------------------------------------------------
Compatibility With Existing Communications Interfaces
-----------------------------------------------------

lwIP
====
`Lightweight IP stack (lwIP)
<https://www.nongnu.org/lwip>`_
is a TCP/IP implementation designed for use on small embedded systems. Some
Pigweed platforms may choose to use lwIP as the backend for their ``Socket``
implementations, so it's important that ``pw::MultiBuf`` interoperate efficiently
with their native buffer type.

lwIP has its own buffer type, `pbuf
<https://www.nongnu.org/lwip/2_1_x/structpbuf.html>`_ optimized for use with
`zero-copy applications
<https://www.nongnu.org/lwip/2_1_x/zerocopyrx.html>`_.
``pbuf`` represents buffers as linked lists of reference-counted ``pbuf`` s
which each have a pointer to their payload, a length, and some metadata. While
this does not precisely match the representation of ``pw::MultiBuf``, it is
possible to construct a ``pbuf`` list which points at the various chunks of a
``pw::MultiBuf`` without needing to perform a copy of the data.

Similarly, a ``pw::MultiBuf`` can refer to a ``pbuf`` by using each ``pbuf`` as
a "``Chunk`` region", holding a reference count on the region's ``pbuf`` so
long as any ``Chunk`` continues to reference the data referenced by that
buffer.

------------------
Existing Solutions
------------------

Linux's ``sk_buff``
===================
Linux has a similar buffer structure called `sk_buff
<https://docs.kernel.org/networking/skbuff.html#struct-sk-buff>`_.
It differs in a few significant ways:

It provides separate ``head``, ``data``, ``tail``, and ``end`` pointers.
Other scatter-gather fragments are supplied using the ``frags[]`` structure.

Separately, it has a ``frags_list`` of IP fragments which is created via calls to
``ip_push_pending_frames``. Fragments are supplied by the ``frags[]`` payload in
addition to the ``skb_shared_info.frag_list`` pointing to the tail.

``sk_buff`` reference-counts not only the underlying chunks of memory, but also the
``sk_buff`` structure itself. This allows for clones of ``sk_buff`` without
manipulating the reference counts of the individual chunks. We anticipate that
cloning a whole ``pw::buffers::MultiBuf`` will be uncommon enough that it is
better to keep these structures single-owner (and mutable) rather than sharing
and reference-counting them.

FreeBSD's ``mbuf``
==================
FreeBSD uses a design called `mbuf
<https://man.freebsd.org/cgi/man.cgi?query=mbuf>`_
which interestingly allows data within the middle of a buffer to be given a
specified alignment, allowing data to be prepended within the same buffer.
This is similar to the structure of ``Chunk``, which may reference data in the
middle of an allocated buffer, allowing prepending without a copy.
