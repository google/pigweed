.. _module-pw_clock_tree:

================
pw_clock_tree
================
.. pigweed-module::
   :name: pw_clock_tree

   - **Constinit compatible**: Clock tree elements can be declared as ``constinit``
   - **Platform independent**: Clock tree management sits above the platform layer
   - **Handles complex topologies**: Clock tree elements can be used to model
     complex clock tree topologies

``pw_clock_tree`` provides class definitions to implement a platform
specific clock tree management solution. By passing the clock tree element
that clocks a particular device to the corresponding device driver, the
device driver can ensure that the clock is only enabled when required to
maximize power savings.

For example the clock tree functionality could be integrated into a uart device driver
like this:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]
   :end-before: // Device constructor that accepts

It could be initialized and used like this:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-IntegrationIntoDeviceDriversUsage]
   :end-before: [pw_clock_tree-examples-IntegrationIntoDeviceDriversUsage]

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart
      :link: module-pw_clock_tree-examples
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Examples that show how to integrate a clock tree into a device driver,
      and how to define and use platform specific clock tree elements.

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: module-pw_clock_tree-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      API references for ``pw::clock_tree::Element``,
      ``pw::clock_tree::DependentElement``, ``pw::clock_tree::ClockTree``,
      and more.


--------
Overview
--------
.. cpp:namespace-push:: pw::clock_tree::ClockTree

Pigweed's clock tree module provides the ability to represent the clock tree of an embedded
device, and to manage the individual clocks and clock tree elements.

The :cpp:class:`ClockTree` implements two basic methods that apply to all clock tree elements:

* :cpp:func:`Acquire`
* :cpp:func:`Release`

In addition, clock divider elements can use the :cpp:func:`SetDividerValue` method to update the current
clock divider value.

.. cpp:namespace-pop::

.. cpp:namespace-push:: pw::clock_tree::Element

The clock tree module defines the :cpp:class:`Element` abstract class from which all
other classes are derived from. The :cpp:class:`Element` abstract class implements basic reference
counting methods :cpp:func:`IncRef` and :cpp:func:`DecRef`, but requires derived classes to use the
reference counting methods to properly acquire and release references to clock
tree elements. If an :cpp:class:`Element` reference is passed to a constructor of a
derived :cpp:class:`Element`, the class object depends on the referenced :cpp:class:`Element` object.

Three derived abstract classes are defined for :cpp:class:`Element`:

.. cpp:namespace-pop::

.. cpp:namespace-push:: pw::clock_tree

* :cpp:class:`ElementBlocking` for clock tree elements that might block when acquired or released.
  Acquiring or releasing such a clock tree element might fail.
* :cpp:class:`ElementNonBlockingCannotFail` for clock tree elements that will not block when
  acquired or released. Acquiring or releasing such a clock tree element will always succeed.
* :cpp:class:`ElementNonBlockingMightFail` for clock tree elements that will not block when
  acquired or released. Acquiring or releasing such a clock tree element might fail.

:cpp:class:`ElementNonBlockingCannotFail` and :cpp:class:`ElementsNonBlockingMightFail` elements can
be acquired from an interrupt context.

All classes representing a clock tree element should be implemented as a template and accept
:cpp:class:`ElementBlocking`, :cpp:class:`ElementNonBlockingCannotFail` or
:cpp:class:`ElementNonBlockingMightFail` as a template argument. Only if it is known at compile time
that a clock tree element can only be either blocking or non-blocking, one can directly derive the
class from :cpp:class:`ElementBlocking` or :cpp:class:`ElementNonBlockingCannotFail` /
:cpp:class:`ElementNonBlockingMightFail`, respectively.

.. cpp:namespace-pop::

---------------
Synchronization
---------------
.. cpp:namespace-push:: pw::clock_tree

The clock tree class uses a mutex to serialize access to :cpp:class:`ElementBlocking` clock tree
elements, and uses an interrupt spin lock to serialize access to
:cpp:class:`ElementNonBlockingCannotFail` and :cpp:class:`ElementNonBlockingMightFail` clock tree
elements. :cpp:class:`ElementBlocking` clock tree elements and
:cpp:class:`ElementNonBlockingCannotFail` / :cpp:class:`ElementNonBlockingMightFail` clock tree
elements are not serialized with each other, while :cpp:class:`ElementNonBlockingCannotFail` and
:cpp:class:`ElementNonBlockingMightFail` are serialized with each other.

.. cpp:namespace-pop::

.. toctree::
   :hidden:
   :maxdepth: 1

   examples
   api
