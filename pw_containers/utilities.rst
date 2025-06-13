.. _module-pw_containers-utilities:

=========
Utilities
=========
.. pigweed-module-subpage::
   :name: pw_containers

In addition to containers, this module includes some types and functions for
working with containers and the data within them.

----------------------------
pw::containers::FilteredView
----------------------------
.. doxygenclass:: pw::containers::FilteredView

-------------------------------
pw::containers::WrappedIterator
-------------------------------
``pw::containers::WrappedIterator`` is a class that makes it easy to wrap an
existing iterator type. It reduces boilerplate by providing ``operator++``,
``operator--``, ``operator==``, ``operator!=``, and the standard iterator
aliases (``difference_type``, ``value_type``, etc.). It does not provide the
dereference operator; that must be supplied by a derived class.

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

---------------------------
pw::containers::PtrIterator
---------------------------
.. doxygenclass:: pw::containers::PtrIterator
.. doxygenclass:: pw::containers::ConstPtrIterator

------------------------
pw::containers::to_array
------------------------
``pw::containers::to_array`` is a C++14-compatible implementation of C++20's
`std::to_array <https://en.cppreference.com/w/cpp/container/array/to_array>`_.
In C++20, it is an alias for ``std::to_array``. It converts a C array to a
``std::array``.

----------
pw::all_of
----------
``pw::all_of`` is a C++17 compatible implementation of C++20's
`std::all_of <https://en.cppreference.com/w/cpp/algorithm/all_any_none_of>`_.
In C++20, it is an alias for ``std::all_of``. This backports the ``constexpr``
overload of the function.

----------
pw::any_of
----------
``pw::any_of`` is a C++17 compatible implementation of C++20's
`std::any_of <https://en.cppreference.com/w/cpp/algorithm/all_any_none_of>`_.
In C++20, it is an alias for ``std::any_of``. This backports the ``constexpr``
overload of the function.

-----------
pw::find_if
-----------
``pw::find_if`` is a C++17 compatible implementation of C++20's
`std::find_if <https://en.cppreference.com/w/cpp/algorithm/find>`_. In C++20, it
is an alias for ``std::find_if``. This backports the ``constexpr`` overload of
the function.

-------------------------
pw_containers/algorithm.h
-------------------------
.. doxygenfile:: pw_containers/algorithm.h
   :sections: detaileddescription

.. doxygenfunction:: pw::containers::AllOf
.. doxygenfunction:: pw::containers::AnyOf
.. doxygenfunction:: pw::containers::NoneOf
.. doxygenfunction:: pw::containers::ForEach
.. doxygenfunction:: pw::containers::Find
.. doxygenfunction:: pw::containers::FindIf
.. doxygenfunction:: pw::containers::FindIfNot
.. doxygenfunction:: pw::containers::FindEnd(Sequence1 &sequence, Sequence2 &subsequence)
.. doxygenfunction:: pw::containers::FindEnd(Sequence1 &sequence, Sequence2 &subsequence, BinaryPredicate &&pred)
.. doxygenfunction:: pw::containers::FindFirstOf(C1& container, C2& options)
.. doxygenfunction:: pw::containers::FindFirstOf(C1& container, C2& options, BinaryPredicate&& pred)
.. doxygenfunction:: pw::containers::AdjacentFind(Sequence& sequence)
.. doxygenfunction:: pw::containers::AdjacentFind(Sequence& sequence, BinaryPredicate&& pred)
.. doxygenfunction:: pw::containers::Count
.. doxygenfunction:: pw::containers::CountIf
.. doxygenfunction:: pw::containers::Mismatch(C1& c1, C2& c2)
.. doxygenfunction:: pw::containers::Mismatch(C1& c1, C2& c2, BinaryPredicate pred)
.. doxygenfunction:: pw::containers::Equal(const C1& c1, const C2& c2)
.. doxygenfunction:: pw::containers::Equal(const C1& c1, const C2& c2, BinaryPredicate&& pred)
.. doxygenfunction:: pw::containers::IsPermutation(const C1& c1, const C2& c2)
.. doxygenfunction:: pw::containers::IsPermutation(const C1& c1, const C2& c2, BinaryPredicate&& pred)
.. doxygenfunction:: pw::containers::Search(Sequence1& sequence, Sequence2& subsequence)
.. doxygenfunction:: pw::containers::Search(Sequence1& sequence, Sequence2& subsequence, BinaryPredicate&& pred)
.. doxygenfunction:: pw::containers::SearchN(Sequence& sequence, Size count, T&& value)
.. doxygenfunction:: pw::containers::SearchN(Sequence& sequence, Size count, T&& value, BinaryPredicate&& pred)
.. doxygenfunction:: pw::all_of(InputIt first, InputIt last, Predicate pred)
.. doxygenfunction:: pw::any_of(InputIt first, InputIt last, Predicate pred)
.. doxygenfunction:: pw::find_if(InputIt first, InputIt last, Predicate pred)
