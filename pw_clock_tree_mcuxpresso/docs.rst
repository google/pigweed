.. _module-pw_clock_tree_mcuxpresso:

===========================
pw_clock_tree_mcuxpresso
===========================
.. pigweed-module::
   :name: pw_clock_tree_mcuxpresso

``pw_clock_tree_mcuxpresso`` implements the ``pw_clock_tree`` interface using the
NXP MCUXpresso SDK. It provides class implementations for the following clock tree elements
that can be directly instantiated:

.. inclusive-language: disable

* Free-Running Oscillator (FRO)
* 32 kHz RTC Oscillator
* Low-power Oscillator
* Master Clock
* External Clock Input as clock source for SYSOSCBYPASS clock selector to generate OSC_CLK
* Fractional Rate Generator (FRG) for Flexcomm Interfaces
* Clock Source Selector
* Clock Divider

.. inclusive-language: enable

Other clock tree components such as PLLs can be instantiated by deriving custom class implementations
from the abstract class `DependentElement` and overwriting `DoEnable` and `DoDisable` methods.

Examples
========

Definition of clock tree elements:

.. mermaid::

    flowchart LR
          A(fro_div_4) -->B(frg_0)
          B-->C(flexcomm_selector_0)
          style A fill:#0f0,stroke:#333,stroke-width:2px
          style B fill:#0f0,stroke:#333,stroke-width:2px
          style C fill:#0f0,stroke:#333,stroke-width:2px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Flexcomm0]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Flexcomm0]

.. mermaid::

    flowchart LR
          D(fro_div_8)--> E(i3c_selector)
          E --> F(i3c_divider)
          style D fill:#f0f,stroke:#333,stroke-width:2px
          style E fill:#f0f,stroke:#333,stroke-width:2px
          style F fill:#f0f,stroke:#333,stroke-width:2px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-i3c0]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-i3c0]

.. mermaid::

    flowchart LR
          G(mclk) --> H(ctimer_0)
          style G fill:#0ff,stroke:#333,stroke-width:2px
          style H fill:#0ff,stroke:#333,stroke-width:2px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Ctimer0]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-Ctimer0]

.. mermaid::

    flowchart LR
          I(lposc)
          style I fill:#ff0,stroke:#333,stroke-width:2px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-LpOsc]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-LpOsc]

Definition of clock tree:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeDef]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeDef]

Example usage of ``clock_tree`` APIs:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-UseExample]
   :end-before: [pw_clock_tree_mcuxpresso-examples-UseExample]

APIs
====

------------------
ClockMcuxpressoFro
------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoFro
   :members:

--------------------
ClockMcuxpressoLpOsc
--------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoLpOsc
   :members:

-------------------
ClockMcuxpressoMclk
-------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoMclk
   :members:

--------------------
ClockMcuxpressoClkIn
--------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoClkIn
   :members:

------------------
ClockMcuxpressoFrg
------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoFrg
   :members:

-----------------------
ClockMcuxpressoSelector
-----------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoSelector
   :members:

----------------------
ClockMcuxpressoDivider
----------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoDivider
   :members:
