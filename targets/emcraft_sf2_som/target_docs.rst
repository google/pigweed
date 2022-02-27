.. _target-emcraft-sf2-som:

-------------------------------------
_target-emcraft-sf2-som: SmartFusion2
-------------------------------------
The Emcraft SmartFusion2 system-on-module target configuration
uses FreeRTOS and the Microchip MSS HAL rather than a from-the-ground-up
baremetal approach.

-----
Setup
-----
To use this target, pigweed must be set up to use FreeRTOS and the Microchip
MSS HAL for the SmartFusion series. The supported repositories can be
downloaded via ``pw package``, and then the build must be manually configured
to point to the locations the repositories were downloaded to.

.. code:: sh

  pw package install freertos
  pw package install smartfusion_mss
  pw package install nanopb

  gn args out
    # Add these lines, replacing ${PW_ROOT} with the path to the location that
    # Pigweed is checked out at.
    dir_pw_third_party_freertos = "${PW_ROOT}/.environment/packages/freertos"
    dir_pw_third_party_smartfusion_mss =
      "${PW_ROOT}/.environment/packages/smartfusion_mss"
    dir_pw_third_party_nanopb = "${PW_ROOT}/.environment/packages/nanopb"

Building and running the demo
=============================
This target does not yet build as part of Pigweed, but will later be
available though the pw_system_demo build target.
