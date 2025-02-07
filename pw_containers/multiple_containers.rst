.. _module-pw_containers-multiple_containers:

------------------------------------
Using items with multiple containers
------------------------------------
Several of Pigweed's containers feature instrusive items, where the data needed
to track where an item is in a container is stored in the item itself. Intrusive
items may be used with multiple containers, provided each of those containers is
templated on a type that is not derived from any of the others. This can be
achieved using multiple inheritance from distinct types:

.. literalinclude:: examples/multiple_containers.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-multiple_containers]
   :end-before: [pw_containers-multiple_containers]

If one or more types is derived from another, the compiler will fail to build
with an error that ``ItemType`` is ambiguous or found in multiple base classes.
