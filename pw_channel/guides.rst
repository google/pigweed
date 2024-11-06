.. _module-pw_channel-quickstart-guides:

===================
Quickstart & guides
===================
.. pigweed-module-subpage::
   :name: pw_channel

.. _module-pw_channel-guides-faqs:

---------------------------------
Frequently asked questions (FAQs)
---------------------------------

My API reads and writes data. Should I implement the ``Channel`` interface, or should my API accept a ``Channel`` to read from and write into?
==============================================================================================================================================
Users should typically only implement the ``Channel`` interface themselves if
they want to control where buffers for underlying data are stored.

On the read side, the channel implementation controls buffer allocation by
choosing how to allocate the ``MultiBuf`` values that are returned from
``PendRead``.

On the write side, the channel implementation controls buffer allocation via
its implementation of the ``PendAllocateWriteBuffer`` method.

Most commonly, lower layers of the communications stack implement ``Channel`` in
order to provide buffers that are customized to the needs of the underlying
transport. For example, a UART DMA channel might need to allocate buffers
out of particular DMA-compatible memory regions, with a particular alignment,
or of a particular MTU size.

The top-level or application layer should typically work with provided
channel implementations in order to allow the lower layers to control
allocation and minimize copies.

Intermediate layers of the stack should generally give preference to
allowing lower layers of the stack to allocate, and so should implement
a channel interface for the higher layer by delegating to the lower layer.
When intermediate layers must copy data (are incompatible with zero-copy),
they should prefer to accept channels from both the higher and lower
layers so that both the application layer and lower layers can choose
how to manage memory allocations.

When necessary, two APIs that accept channels can be paired together
using a ``ForwardingChannel`` which manages allocations by delegating
to the provided ``MultiBufAllocator``.

Can different tasks write into and read from the same channel?
==============================================================
No; it is not possible to read from the channel in one task while
writing to it from another task. A single task must own and operate
the channel. In the future, a wrapper will be offered which will
allow the channel to be split into a read half and a write half which
can be used from independent tasks.
