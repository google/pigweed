.. _module-pw_containers-vectors:

=======
Vectors
=======
.. pigweed-module-subpage::
   :name: pw_containers

A vector is a one-dimensional array with a variable length. Pigweed's vector
type differs from the standard library in that it is backed by a fixed-size
buffer.

----------
pw::Vector
----------
Vectors must be declared with an explicit maximum size
(e.g. ``Vector<int, 10>``) but vectors can be used and referred to without the
max size template parameter (e.g. ``Vector<int>``).

To allow referring to a ``pw::Vector`` without an explicit maximum size, all
Vector classes inherit from the generic ``Vector<T>``, which stores the maximum
size in a variable. This allows Vectors to be used without having to know
their maximum size at compile time. It also keeps code size small since
function implementations are shared for all maximum sizes.

Example
=======
.. literalinclude:: examples/vector.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-vector]
   :end-before: [pw_containers-vector]

API reference
=============
The Vector class is similar to ``std::vector``, except it is backed by a
fixed-size buffer.

.. doxygenclass:: pw::Vector
   :members:


Size report
===========
.. TODO: b/388905812 - Re-enable the size report.
.. .. include:: vectors_size_report
.. include:: ../size_report_notice
