.. _module-pw_containers-sets:

====
Sets
====
.. pigweed-module-subpage::
   :name: pw_containers

A set is an unordered collection of items. Pigweed provides implementations that
can insert, find, and remove items in logarithmic time.

.. _module-pw_containers-intrusive_set:

----------------
pw::IntrusiveSet
----------------
``pw::IntrusiveSet`` provides an embedded-friendly, tree-based, intrusive
set implementation. The intrusive aspect of the set is very similar to that of
:ref:`module-pw_containers-intrusive_list`.

This class is similar to ``std::set<T>``. Items to be added must derive from
``pw::IntrusiveSet<T>::Item`` or an equivalent type.

See also :ref:`module-pw_containers-multiple_containers`.

Example
=======
.. literalinclude:: examples/intrusive_set.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-intrusive_set]
   :end-before: [pw_containers-intrusive_set]

If you need to add this item to containers of more than one type, see
:ref:`module-pw_containers-multiple_containers`,

---------------------
pw::IntrusiveMultiSet
---------------------
``pw::IntrusiveMultiSet`` provides an embedded-friendly, tree-based, intrusive
multiset implementation. This is very similar to
:ref:`module-pw_containers-intrusive_set`, except that the tree may contain
multiple items with equivalent keys.

This class is similar to ``std::multiset<T>``. Items to be added must derive
from ``pw::IntrusiveMultiSet<T>::Item`` or an equivalent type.

See also :ref:`module-pw_containers-multiple_containers`.

Example
=======
.. literalinclude:: examples/intrusive_multiset.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-intrusive_multiset]
   :end-before: [pw_containers-intrusive_multiset]

If you need to add this item to containers of more than one type, see
:ref:`module-pw_containers-multiple_containers`,

-------------
API reference
-------------
Moved: :cc:`pw_containers_sets`

------------
Size reports
------------
The tables below illustrate the following scenarios:

* Scenarios related to ``IntrusiveSet``:

  * The memory and code size cost incurred by a adding a single
    ``IntrusiveSet``.
  * The memory and code size cost incurred by adding another ``IntrusiveSet``
    with a different type. As ``IntrusiveSet`` is templated on type, this
    results in additional code being generated.

* Scenarios related to ``IntrusiveMultiSet``:

  * The memory and code size cost incurred by a adding a single
    ``IntrusiveMultiSet``.
  * The memory and code size cost incurred by adding another
    ``IntrusiveMultiSet`` with a different type. As ``IntrusiveMultiSet`` is
    templated on type, this results in additional code being generated.

* The memory and code size cost incurred by a adding both an ``IntrusiveSet``
  and an ``IntrusiveMultiSet`` of the same type. These types reuse code, so the
  combined sum is less than the sum of its parts.

.. include:: sets_size_report
