.. _module-pw_clock_tree-examples:

--------
Examples
--------
.. pigweed-module-subpage::
   :name: pw_clock_tree

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Integration into device driver
      :link: module-pw_clock_tree-example-device_driver
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Example that shows how to integrate the clock tree functionality
      into a device driver.

   .. grid-item-card:: :octicon:`code-square` Clock tree usage
      :link: module-pw_clock_tree-example-clock_tree_usage
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Example that shows how to define platform specific clock tree elements
      and how to interact with them to manage clocks of an embedded system.


.. _module-pw_clock_tree-example-device_driver:

Clock tree integration into device drivers
==========================================

The example below shows how the clock tree functionality can get integrated into a new
device driver that requires that a clock tree abstraction is present in the system.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]
   :end-before: [pw_clock_tree-examples-IntegrationIntoDeviceDriversClassDef]


.. _module-pw_clock_tree-example-clock_tree_usage:

Definition and use of clock tree elements
=========================================

For the example below we use a clock tree with two clock sources ``clock_a`` and ``clock_b``.
``clock_a`` can be selected as an input source by ``clock_selector_c``, and ``clock_b`` is an input into
divider ``clock_divider_d``, which can be selected as an alternative input source by
``clock_selector_c``.

.. mermaid::

    flowchart LR
          A(clock_a) -..-> C(clock_selector_c)
          B(clock_b)--> D(clock_divider_d)
          D -..-> C

.. cpp:namespace-push:: pw::clock_tree::Element

Derived ``ClockSourceExample`` class template that overrides
:cpp:func:`DoEnable` and :cpp:func:`DoDisable` methods.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ClockSourceExampleDef]
   :end-before: [pw_clock_tree-examples-ClockSourceExampleDef]

Derived ``ClockDividerExample`` class template that overrides
:cpp:func:`DoEnable` method.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ClockDividerExampleDef]
   :end-before: [pw_clock_tree-examples-ClockDividerExampleDef]

Derived ``ClockSelectorExample`` class template that overrides
:cpp:func:`DoEnable` and :cpp:func:`DoDisable` methods,
and defines the ``SetSource`` method to allow the clock selector to change from one dependent source to
another source. If the dependent source of a clock selector doesn't change at any point, one doesn't
need to implement a method like ``SetSource``.

.. cpp:namespace-pop::

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ClockSelectorExampleDef]
   :end-before: [pw_clock_tree-examples-ClockSelectorExampleDef]

.. cpp:namespace-push:: pw::clock_tree

Derived ``ClockTreeSetSource`` class that provides ``SetSource`` method to allow to change the
source a clock selector depends on. If ``ClockSelectorExample`` wouldn't provide the ``SetSource``
method, one could use the :cpp:class:`ClockTree` class directly in the example below.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ClockTreeSetSourcesExampleDef]
   :end-before: [pw_clock_tree-examples-ClockTreeSetSourcesExampleDef]

Declare the :cpp:class:`ClockTree` class object.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ClockTreeDec]
   :end-before: [pw_clock_tree-examples-ClockTreeDec]

Declare the clock tree elements.
``clock_selector_c`` depends on ``clock_a``, and ``clock_divider_d`` depends on ``clock_b``.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ClockTreeElementsDec]
   :end-before: [pw_clock_tree-examples-ClockTreeElementsDec]

Acquire a reference to ``clock_selector_c``, which will acquire a reference to the dependent source
``clock_a``. When the reference to ``clock_a`` gets acquired, ``clock_a`` gets enabled. Once the
reference to ``clock_a`` has been acquired and it is enabled, ``clock_selector_c`` gets enabled.

.. mermaid::

    flowchart LR
          A(clock_A) -->C(clock_selector_c)
          B(clock_B)--> D(clock_divider_d)
          D -..-> C
          style A fill:#0f0,stroke:#333,stroke-width:4px
          style C fill:#0f0,stroke:#333,stroke-width:4px
          style B fill:#f00,stroke:#333,stroke-width:4px
          style D fill:#f00,stroke:#333,stroke-width:4px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-AcquireClockSelectorC]
   :end-before: [pw_clock_tree-examples-AcquireClockSelectorC]

Change the dependent source of ``clock_selector_c`` from ``clock_a`` to ``clock_divider_d`` while
the ``clock_selector_c`` is enabled. Before ``clock_divider_d`` can be configured as the new
dependent source, a reference to ``clock_divider_d`` will need to get acquired, which will acquire
a reference to ``clock_b`` and enable ``clock_b`` before ``clock_divider_d`` gets enabled.
Once the dependent source has been changed from ``clock_a`` to ``clock_divider_d``, the reference to
``clock_a`` will get released, which will disable ``clock_a``.

.. mermaid::

    flowchart LR
          A(clock_A) -..->C(clock_selector_c)
          B(clock_B)--> D(clock_divider_d)
          D --> C
          style A fill:#f00,stroke:#333,stroke-width:4px
          style C fill:#0f0,stroke:#333,stroke-width:4px
          style B fill:#0f0,stroke:#333,stroke-width:4px
          style D fill:#0f0,stroke:#333,stroke-width:4px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ChangeClockSelectorCDependentSource]
   :end-before: [pw_clock_tree-examples-ChangeClockSelectorCDependentSource]

Set the clock divider value while the ``clock_divider_d`` is enabled.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-SetClockDividerDValue]
   :end-before: [pw_clock_tree-examples-SetClockDividerDValue]

Release the reference to the ``clock_selector_c``, which will disable ``clock_selector_c``, and
then release the reference to ``clock_divider_d``. Then ``clock_divider_d`` will get disabled before
it releases its reference to ``clock_b`` that gets disabled afterward. At this point all clock
tree elements are disabled.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree-examples-ReleaseClockSelectorC]
   :end-before: [pw_clock_tree-examples-ReleaseClockSelectorC]
