.. _module-pw_containers-utilities:

=========
Utilities
=========
.. pigweed-module-subpage::
   :name: pw_containers

In addition to containers, this module includes some types and functions for
working with containers and the data within them.

-------------
API reference
-------------
Moved: :cc:`pw_containers_utilities`

-------------------------------
pw::containers::WrappedIterator
-------------------------------
:cc:`pw::containers::WrappedIterator` is a class that makes it easy to
wrap an existing iterator type. It reduces boilerplate by providing
``operator++``, ``operator--``, ``operator==``, ``operator!=``, and the
standard iterator aliases (``difference_type``, ``value_type``, etc.). It does
not provide the dereference operator; that must be supplied by a derived class.

Example
=======
To use it, create a class that derives from ``WrappedIterator`` and define
``operator*()`` and ``operator->()`` as appropriate. The new iterator might
apply a transformation to or access a member of the values provided by the
original iterator. The following example defines an iterator that multiplies the
values in an array by 2.

.. literalinclude:: examples/wrapped_iterator.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-wrapped_iterator]
   :end-before: [pw_containers-wrapped_iterator]

Basic functional programming
============================
``WrappedIterator`` may be used in concert with ``FilteredView`` to create a
view that iterates over a matching values in a container and applies a
transformation to the values. For example, it could be used with
``FilteredView`` to filter a list of packets and yield only one field from the
packet.

The combination of ``FilteredView`` and ``WrappedIterator`` provides some basic
functional programming features similar to (though much more cumbersome than)
`generator expressions <https://www.python.org/dev/peps/pep-0289/>`_ (or `filter
<https://docs.python.org/3/library/functions.html#filter>`_/`map
<https://docs.python.org/3/library/functions.html#map>`_) in Python or streams
in Java 8. ``WrappedIterator`` and ``FilteredView`` require no memory
allocation, which is helpful when memory is too constrained to process the items
into a new container.

-----------------
pw::OptionalTuple
-----------------
:cc:`pw::OptionalTuple` is an efficient tuple implementation with optional
elements, similar to ``std::tuple<std::optional<Types>...>``. Field presence is
tracked by a single :cc:`pw::BitSet`, instead of a flag in each type.

Example
=======
.. literalinclude:: examples/optional_tuple.cc
   :language: cpp
   :linenos:
   :start-at: #include
   :end-at: }  // namespace example

---------------
C++20 polyfills
---------------
:cs:`pw_containers/public/pw_containers/to_array.h` provides
:cc:`pw::containers::to_array`, a C++17-compatible implementation of C++20's
`std::to_array <https://en.cppreference.com/w/cpp/container/array/to_array>`_.
In C++20, it is an alias for ``std::to_array``. It converts a C array to a
``std::array``.

:cs:`pw_containers/public/pw_containers/algorithm.h` provides ``constexpr``
implementations of  various ``<algorithm>`` functions including :cc:`pw::copy`,
:cc:`pw::all_of`, :cc:`pw::any_of`, and :cc:`pw::find_if`.
