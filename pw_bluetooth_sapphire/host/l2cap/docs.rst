=====
L2CAP
=====

------------------
Supported Features
------------------
.. supported-features-start

.. list-table::
   :header-rows: 1

   * - Feature
     - Supported?
     - Notes
   * - Fragmentation/Recombination
     - ✅
     -
   * - Segmentation/Reassembly
     - ❌
     - `Tracking bug <https://fxbug.dev/372271169>`__ Clients must respect the
       peer's MTU size, and Sapphire supports receiving the max SDU size.
   * - BR/EDR Dynamic Channels
     - ✅
     -
   * - LE Connection-Oriented Channels (CoC)
     - ✅
     -
   * - Basic Mode (ERTM)
     - ✅
     -
   * - Enhanced Retransmission Mode (ERTM)
     - ✅
     -
   * - LE Credit-Based Flow Control Mode
     - ✅
     -
   * - Retransmission Mode
     - ❌
     - `Tracking bug <https://fxbug.dev/372271169>`__
   * - Flow Control Mode
     - ❌
     - `Tracking bug <https://fxbug.dev/372274047>`__
   * - Streaming Mode
     - ❌
     - `Tracking bug <https://fxbug.dev/372274603>`__

.. supported-features-end
