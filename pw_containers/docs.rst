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
IntrusiveList provides an embedded-friendly singly-linked list implementation.
An intrusive list is a type of linked list that embeds the "next" pointer into
the list object itself. This allows the construction of a linked list without
the need to dynamically allocate list entries to point to the actual in-memory
objects. In C, an intrusive list can be made by manually including the "next"
pointer as a member of the object's struct. `pw::IntrusiveList` uses C++
features to simplify the process of creating an intrusive list and intrusive
list objects by providing a class that list elements can inherit from. This
protects the "next" pointer from being accessed by the actual item that is
stored in the linked list; only the `pw::IntrusiveList` class can modify the
list.


Usage
-----
While the API of `pw::IntrusiveList` is relatively similar to a
``std::forward_list``, there are extra steps to creating objects that can be
stored in this data structure. Objects that will be added to a
``IntrusiveList<T>`` must inherit from ``IntrusiveList<T>::Item``. When an item
is instantiated and added to a linked list, the pointer to the object is added
to the "next" pointer of whichever object is the current tail.


That means two key things:

 - An instantiated IntrusiveList::Item must remain in scope for the lifetime of
   the IntrusiveList it has been added to.
 - A linked list item CANNOT be included in two lists, as it is part of a
   preexisting list and adding it to another implicitly breaks correctness
   of the first list.

.. code-block:: cpp

  class Square
     : public pw::containers::IntrusiveList<Square>::Item {
   public:
    Square(unsigned int side_length) : side_length(side_length) {}
    unsigned long Area() { return side_length * side_length; }

   private:
    unsigned int side_length;
  };

  pw::containers::IntrusiveList<Square> squares;

  Square small(1);
  Square large(4000);
  // These elements are not copied into the linked list, the original objects
  // are just chained together and can be accessed via
  // `IntrusiveList<Square> squares`.
  squares.push_back(small);
  squares.push_back(large);

  {
    // ERROR: When this goes out of scope, it will break the linked list.
    Square different_scope = Square(5);
    squares.push_back(&different_scope);
  }

  for (auto& square : squares) {
    PW_LOG_INFO("Found a square with an area of %ul", square.Area());
  }


Compatibility
=============
* C
* C++17

Dependencies
============
* ``pw_span``
