.. _module-pw_multibuf:

===========
pw_multibuf
===========
.. pigweed-module::
   :name: pw_multibuf

Sending or receiving messages via RPC, transfer, or sockets often requires a
series of intermediate buffers, each requiring their own copy of the data.
``pw_multibuf`` allows data to be written *once*, eliminating the memory, CPU
and latency overhead of copying.

-----------------
How does it work?
-----------------
``pw_multibuf`` uses several techniques to minimize copying of data:

- **Header and Footer Reservation**: Lower-level components can reserve space
  within a buffer for headers and/or footers. This allows headers and footers
  to be added to user-provided data without moving users' data.
- **Native Scatter/Gather and Fragmentation Support**: Buffers can refer to
  multiple separate chunks of memory. Messages can be built up from
  discontiguous allocations, and users' data can be fragmented across multiple
  packets.
- **Divisible Memory Regions**: Incoming buffers can be divided without a copy,
  allowing incoming data to be freely demultiplexed.

-------------------------------
What kinds of data is this for?
-------------------------------
``pw_multibuf`` is best used in code that wants to read, write, or pass along
data which are one of the following:

- **Large**: ``pw_multibuf`` is designed to allow breaking up data into
  multiple chunks. It also supports asynchronous allocation for when there may
  not be sufficient space for incoming data.
- **Communications-Oriented**: Data which is being received or sent across
  sockets, various packets, or shared-memory protocols can benefit from the
  fragmentation, multiplexing, and header/footer-reservation properties of
  ``pw_multibuf``.
- **Copy-Averse**: ``pw_multibuf`` is structured to allow users to pass around
  and mutate buffers without copying or moving data in-memory. This can be
  especially useful when working in systems that are latency-sensitive,
  need to pass large amounts of data, or when memory usage is constrained.

---------------------------
Why are there two versions?
---------------------------
We are currently investigating an alternate approach that keeps many of the
aspects described above, while separating out the concern of memory allocation
and instead using ``MultiBuf`` to present different logical, span-like views of
sequences of buffers. This version is currently experimental, but we welcome
feedback on the direction it is taking!

-------------
API reference
-------------
Moved: :doxylink:`pw_multibuf`

.. toctree::
   :hidden:

   code_size
