.. _module-pw_containers:

-------------
pw_containers
-------------
The ``pw_containers`` module provides embedded-friendly container classes.

pw::Vector
==========
The Vector class is similar to ``std::vector``, except it is backed by a
fixed-size buffer. Vectors must be declared with an explicit maximum size
(e.g. ``Vector<int, 10>``) but vectors can be used and referred to without the
max size template parameter (e.g. ``Vector<int>``).

To allow referring to a ``pw::Vector`` without an explicit maximum size, all
Vector classes inherit from the generic ``Vector<T>``, which stores the maximum
size in a variable. This allows Vectors to be used without having to know
their maximum size at compile time. It also keeps code size small since
function implementations are shared for all maximum sizes.

pw::IntrusiveList
=================
IntrusiveList provides an embedded-friendly singly-linked intrusive list
implementation. An intrusive list is a type of linked list that embeds the
"next" pointer into the list object itself. This allows the construction of a
linked list without the need to dynamically allocate list entries.

In C, an intrusive list can be made by manually including the "next" pointer as
a member of the object's struct. ``pw::IntrusiveList`` uses C++ features to
simplify the process of creating an intrusive list. ``pw::IntrusiveList``
provides a class that list elements can inherit from. This protects the "next"
pointer from being accessed by the item class, so only the ``pw::IntrusiveList``
class can modify the list.

Usage
-----
While the API of ``pw::IntrusiveList`` is similar to a ``std::forward_list``,
there are extra steps to creating objects that can be stored in this data
structure. Objects that will be added to a ``IntrusiveList<T>`` must inherit
from ``IntrusiveList<T>::Item``. They can inherit directly from it or inherit
from it through another base class. When an item is instantiated and added to a
linked list, the pointer to the object is added to the "next" pointer of
whichever object is the current tail.

That means two key things:

 - An instantiated ``IntrusiveList<T>::Item`` must remain in scope for the
   lifetime of the ``IntrusiveList`` it has been added to.
 - A linked list item CANNOT be included in two lists. Attempting to do so
   results in an assert failure.

.. code-block:: cpp

  class Square
     : public pw::IntrusiveList<Square>::Item {
   public:
    Square(unsigned int side_length) : side_length(side_length) {}
    unsigned long Area() { return side_length * side_length; }

   private:
    unsigned int side_length;
  };

  pw::IntrusiveList<Square> squares;

  Square small(1);
  Square large(4000);
  // These elements are not copied into the linked list, the original objects
  // are just chained together and can be accessed via
  // `IntrusiveList<Square> squares`.
  squares.push_back(small);
  squares.push_back(large);

  {
    // When different_scope goes out of scope, it removes itself from the list.
    Square different_scope = Square(5);
    squares.push_back(&different_scope);
  }

  for (const auto& square : squares) {
    PW_LOG_INFO("Found a square with an area of %lu", square.Area());
  }

pw::containers::FlatMap
=======================
FlatMap provides a simple, fixed-size associative array with lookup by key or
value. ``pw::containers::FlatMap`` contains the same methods and features for
looking up data as std::map. However, there are no methods that modify the
underlying data.  The underlying array in ``pw::containers::FlatMap`` does not
need to be sorted. During construction, ``pw::containers::FlatMap`` will
perform a constexpr insertion sort.

pw::containers::FilteredView
============================
``pw::containers::FilteredView`` provides a view of a container that only
contains elements that match the specified filter. This class is similar to
C++20's `std::ranges::filter_view
<https://en.cppreference.com/w/cpp/ranges/filter_view>`_.

To create a ``FilteredView``, pass a container and a filter object, which may be
a lambda or class that implements ``operator()`` for the container's value type.

.. code-block:: cpp

  std::array<int, 99> kNumbers = {3, 1, 4, 1, ...};

  for (int even : FilteredView(kNumbers, [](int n) { return n % 2 == 0; })) {
    PW_LOG_INFO("This number is even: %d", even);
  }

Compatibility
=============
* C++17

Dependencies
============
* ``pw_span``
