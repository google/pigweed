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
* External Clock Input
* Fractional Rate Generator (FRG) for Flexcomm Interfaces
* Clock Source Selector
* Clock Divider

.. inclusive-language: enable

Other clock tree components such as PLLs can be instantiated by deriving custom class implementations
from the abstract class `DependentElement` and overwriting `DoEnable` and `DoDisable` methods.

Examples
==============

.. inclusive-language: disable

.. code-block:: cpp

   // Define FRO_DIV_4 clock source
   constinit ClockMcuxpressoFro fro_div_4(kCLOCK_FroDiv4OutEn);

   // Define FRO_DIV_8 clock source
   constinit ClockMcuxpressoFro fro_div_8(kCLOCK_FroDiv8OutEn);

   // Define Low-Power Oscillator
   constinit ClockMcuxpressoLpOsc lp_osc_clk;

   // Define Master clock
   constinit ClockMcuxpressoMclk mclk(19200000);

   // Define extern clock input
   constinit ClockMcuxpressoClkIn clk_in(25000000);

   // Define FRG0 configuration
   const clock_frg_clk_config_t g_frg0Config_BOARD_BOOTCLOCKRUN =
   {
        .num = 0,
        .sfg_clock_src = _clock_frg_clk_config::kCLOCK_FrgFroDiv4,
        .divider = 255U,
        .mult = 144
   };

   constinit ClockMcuxpressoFrgNonBlocking frg0(fro_div_4, g_frg0Config_BOARD_BOOTCLOCKRUN);

   // Define clock source selector I3C01FCLKSEL
   constinit ClockMcuxpressoSelectorNonBlocking i3c0_selector(fro_div_8,
                                                              kFRO_DIV8_to_I3C_CLK,
                                                              kNONE_to_I3C_CLK);

   // Define clock divider I3C01FCLKDIV
   constinit ClockMcuxpressoDividerNonBlocking i3c0_divider(i3c0_selector,
                                                            kCLOCK_DivI3cClk,
                                                            12);

   // Create the clock tree
   ClockTree clock_tree;

   // Enable the low-power oscillator
   clock_tree.Acquire(lp_osc_clk);

   // Enable the i3c0_divider
   clock_tree.Acquire(i3c0_divider);

   // Change the i3c0_divider value
   PW_TRY(clock_tree.SetDividerValue(i3c0_divider, 24));

   // Disable the low-power oscillator
   clock_tree.Release(lp_osc_clk);

.. inclusive-language: enable

ClockMcuxpressoFro
==================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoFro
   :members:

ClockMcuxpressoLpOsc
====================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoLpOsc
   :members:

ClockMcuxpressoMclk
===================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoMclk
   :members:

ClockMcuxpressoClkIn
====================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoClkIn
   :members:

ClockMcuxpressoFrg
==================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoFrg
   :members:

ClockMcuxpressoSelector
=======================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoSelector
   :members:

ClockMcuxpressoDivider
======================
.. doxygenclass:: pw::clock_tree::ClockMcuxpressoDivider
   :members:
