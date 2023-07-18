.. _target-emcraft-sf2-som:

--------------------
Emcraft SmartFusion2
--------------------
The Emcraft SmartFusion2 system-on-module target configuration
uses FreeRTOS and the Microchip MSS HAL rather than a from-the-ground-up
baremetal approach.


Setup
=====
To use this target, pigweed must be set up to use FreeRTOS and the Microchip
MSS HAL for the SmartFusion series. The supported repositories can be
downloaded via ``pw package``, and then the build must be manually configured
to point to the locations the repositories were downloaded to.

.. code:: sh

  pw package install freertos
  pw package install smartfusion_mss
  pw package install nanopb

  gn args out

Then add the following lines to that text file:

.. code::

    dir_pw_third_party_freertos = getenv("PW_PACKAGE_ROOT") + "/freertos"
    dir_pw_third_party_smartfusion_mss = getenv("PW_PACKAGE_ROOT") + "/smartfusion_mss"
    dir_pw_third_party_nanopb = getenv("PW_PACKAGE_ROOT") + "/nanopb"

Building and running the demo
=============================
This target does not yet build as part of Pigweed, but will later be
available though the pw_system_demo build target.
