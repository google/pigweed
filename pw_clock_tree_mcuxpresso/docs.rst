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
* Audio PLL
* RTC

.. inclusive-language: enable

.. cpp:namespace-push:: pw::clock_tree::Element

Other clock tree components such as PLLs can be instantiated by deriving custom class implementations
from the abstract class :cpp:class:`DependentElement` and overwriting :cpp:func:`DoEnable` and
:cpp:func:`DoDisable` methods.

.. cpp:namespace-pop::

.. cpp:namespace-push:: pw::clock_tree::ClockTree

When enabling clock tree elements sourced from the audio PLL or the SYS PLL it is necessary
to use the :cpp:func:`AcquireWith` method.

.. cpp:namespace-pop::

Examples
========

----------------------------------------
End-to-end Mcuxpresso clock tree example
----------------------------------------

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
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-fro_div8]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-fro_div8]

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
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClkTreeElemDefs-ClockSourceNoOp]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClkTreeElemDefs-ClockSourceNoOp]

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

------------------
Audio PLL examples
------------------

.. cpp:namespace-push:: pw::clock_tree::ClockTree

The :cpp:class:`ClockMcuxpressoAudioPll` can be configured in two different ways,
either it can be configured where the audio PLL gets enabled, or it can be
configured in bypass mode.

The first example shows where :cpp:class:`ClockMcuxpressoAudioPll` enables the audio PLL
and uses the ClkIn pin clock source as OSC clock source that feeds into the audio PLL
logic. Since the audio PLL requires that the ``FRO_DIV8`` clock source is enabled when
enabling the audio PLL, the :cpp:func:`AcquireWith` needs to be used that ensures
that the ``FRO_DIV8`` clock is enabled when enabling the audio PLL.

.. mermaid::

    flowchart LR
          subgraph PLL [Audio PLL logic]
          B(audio_pll_selctor) -.-> C(PLL)
          C -.-> D(Phase Fraction divider)
          end
          A(clk_in) -->|as osc_clk| PLL
          PLL --> E(audio_pfd_bypass_selector)

          style A fill:#f0f,stroke:#333,stroke-width:2px
          style E fill:#f0f,stroke:#333,stroke-width:2px

Definition of clock tree:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeDef]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeDef]

Definition of audio PLL related clock tree elements:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClkTreeElemDefs-ClockSourceNoOp]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClkTreeElemDefs-ClockSourceNoOp]

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPll]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPll]

Definition of ``FRO_DIV8`` clock tree element:

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-fro_div8]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElementDefs-fro_div8]

Audio PLL clock tree element gets enabled and disabled. We use :cpp:func:`AcquireWith`
to ensure that ``FRO_DIV8`` is enabled prior to configuring the audio PLL to a
non-``FRO_DIV8`` clock source.

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-Use-AudioPll]
   :end-before: [pw_clock_tree_mcuxpresso-examples-Use-AudioPll]

The second example shows where :cpp:class:`ClockMcuxpressoAudioPll` bypasses the audio PLL
and uses ``FRO_DIV8 pin`` clock source.

.. cpp:namespace-pop::

.. mermaid::

    flowchart LR
          A(fro_div_8) --> B(audio_pfd_bypass_selector)
          style A fill:#0ff,stroke:#333,stroke-width:2px
          style B fill:#0ff,stroke:#333,stroke-width:2px

.. literalinclude:: examples.cc
   :language: cpp
   :linenos:
   :start-after: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPllBypass]
   :end-before: [pw_clock_tree_mcuxpresso-examples-ClockTreeElemDefs-AudioPllBypass]

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

-----------------------
ClockMcuxpressoAudioPll
-----------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoAudioPll
   :members:

------------------
ClockMcuxpressoRtc
------------------
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoRtc
   :members:
