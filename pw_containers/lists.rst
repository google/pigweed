.. _module-pw_containers-lists:

=====
Lists
=====
.. pigweed-module-subpage::
   :name: pw_containers

A linked list is an ordered collection of items in which each item is associated
with pointers to one or more of its adjacent items. Pigweed provides intrusive
lists, meaning the pointers are stored within the items themselves. This allows
an arbitrary number of items to be added to a list without requiring additional
memory beyond that of the items themselves.

.. _module-pw_containers-intrusive_list:

-----------------
pw::IntrusiveList
-----------------
``pw::IntrusiveList`` provides an embedded-friendly, double-linked, intrusive
list implementation. An intrusive list is a type of linked list that embeds list
metadata, such as a "next" pointer, into the list object itself. This allows the
construction of a linked list without the need to dynamically allocate list
entries.

In C, an intrusive list can be made by manually including the "next" pointer as
a member of the object's struct. ``pw::IntrusiveList`` uses C++ features to
simplify the process of creating an intrusive list. It provides classes that
list elements can inherit from, protecting the list metadata from being accessed
by the item class.

See also :ref:`module-pw_containers-multiple_containers`.

.. _module-pw_containers-intrusive_list-example:

Example
=======
.. literalinclude:: examples/intrusive_list.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-intrusive_list]
   :end-before: [pw_containers-intrusive_list]

If you need to add this item to containers of more than one type, see
:ref:`module-pw_containers-multiple_containers`,

API reference
=============
This class is similar to ``std::list<T>``, except that the type of items to be
added must derive from ``pw::IntrusiveList<T>::Item``.

.. doxygenclass:: pw::containers::future::IntrusiveList
   :members:

.. note::
   Originally, ``pw::IntrusiveList<T>`` was implemented as a singly linked list.
   To facilitate migration to ``pw::IntrusiveForwardList<T>::Item``, this
   original implementation can still be temporarily used by enabling the
   ``PW_CONTAINERS_USE_LEGACY_INTRUSIVE_LIST`` module configuration option.

------------------------
pw::IntrusiveForwardList
------------------------
``pw::IntrusiveForwardList`` provides an embedded-friendly, singly linked,
intrusive list implementation. It is very similar to
:ref:`module-pw_containers-intrusive_list`, except that it is singly rather than
doubly linked.

See also :ref:`module-pw_containers-multiple_containers`.

Example
=======
.. literalinclude:: examples/intrusive_forward_list.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-intrusive_forward_list]
   :end-before: [pw_containers-intrusive_forward_list]

If you need to add this item to containers of more than one type, see
:ref:`module-pw_containers-multiple_containers`,

API reference
=============
This class is similar to ``std::forward_list<T>``. Items to be added must derive
from ``pw::IntrusiveForwardList<T>::Item``.

.. doxygenclass:: pw::IntrusiveForwardList
   :members:

Performance considerations
==========================
Items only include pointers to the next item. To reach previous items, the list
maintains a cycle of items so that the first item can be reached from the last.
This structure means certain operations have linear complexity in terms of the
number of items in the list, i.e. they are "O(n)":

- Removing an item from a list using
  ``pw::IntrusiveForwardList<T>::remove(const T&)``.
- Getting the list size using ``pw::IntrusiveForwardList<T>::size()``.

When using a ``pw::IntrusiveForwardList<T>`` in a performance-critical section
or with many items, authors should prefer to avoid these methods. For example,
it may be preferable to create items that together with their storage outlive
the list.

Notably, ``pw::IntrusiveForwardList<T>::end()`` is constant complexity (i.e.
"O(1)"). As a result iterating over a list does not incur an additional penalty.

.. _module-pw_containers-intrusivelist-size-report:

Size reports
------------
The tables below illustrate the following scenarios:

* Scenarios related to ``IntrusiveList``:

  * The memory and code size cost incurred by a adding a single
    ``IntrusiveList``.
  * The memory and code size cost incurred by adding another ``IntrusiveList``
    with a different type. As ``IntrusiveList`` is templated on type, this
    results in additional code being generated.

* Scenarios related to ``IntrusiveForwardList``:

  * The memory and code size cost incurred by a adding a single
    ``IntrusiveForwardList``.
  * The memory and code size cost incurred by adding another
    ``IntrusiveForwardList`` with a different type. As ``IntrusiveForwardList``
    is templated on type, this results in additional code being generated.

* The memory and code size cost incurred by a adding both an ``IntrusiveList``
  and an ``IntrusiveForwardList`` of the same type. These types reuse code, so
  the combined sum is less than the sum of its parts.

.. TODO: b/388905812 - Re-enable the size report.
.. .. include:: lists_size_report
.. include:: ../size_report_notice
