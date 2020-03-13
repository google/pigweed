.. default-domain:: cpp

.. highlight:: sh

-------------
pw_containers
-------------
The ``pw_containers`` module provides embedded-friendly container classes.

pw_vector
=========
The Vector class is similar to std::vector, except it is backed by a
fixed-size buffer. Vectors must be declared with an explicit maximum size
(e.g. ``Vector<int, 10>``) but vectors can be used and referred to without the
max size template parameter (e.g. ``Vector<int>``).

To allow referring to a ``pw::Vector`` without an explicit maximum size, all
Vector classes inherit from the generic ``Vector<T>``, which stores the maximum
size in a variable. This allows Vectors to be used without having to know
their maximum size at compile time. It also keeps code size small since
function implementations are shared for all maximum sizes.

Compatibility
=============
* C
* C++17

Dependencies
============
* ``pw_span``
