.. _module-pw_third_party_freertos:

========
FreeRTOS
========

The ``$dir_pw_third_party/freertos/`` module contains various helpers to use
FreeRTOS, including Pigweed backend modules which depend on FreeRTOS.

----------------
GN Build Support
----------------
This module provides support to compile FreeRTOS with GN. This is required when
compiling backends modules for FreeRTOS.

In order to use this you are expected to configure the following variables from
``$dir_pw_third_party/freertos:freertos.gni``:

#. Set the GN ``dir_pw_third_party_freertos`` to the path of the FreeRTOS
   installation.
#. Set ``pw_third_party_freertos_CONFIG`` to a ``pw_source_set`` which provides
   the FreeRTOS config header.
#. Set ``pw_third_party_freertos_PORT`` to a ``pw_source_set`` which provides
   the FreeRTOS port specific includes and sources.

After this is done a ``pw_source_set`` for the FreeRTOS library is created at
``$dir_pw_third_party/freertos``.

-----------------------------
OS Abstraction Layers Support
-----------------------------
Support for Pigweed's :ref:`docs-os_abstraction_layers` are provided for
FreeRTOS via the following modules:

* :ref:`module-pw_chrono_freertos`
* :ref:`module-pw_sync_freertos`
* :ref:`module-pw_thread_freertos`

--------------------------
configASSERT and pw_assert
--------------------------
To make it easier to use :ref:`module-pw_assert` with FreeRTOS a helper header
is provided under ``pw_third_party/freertos/config_assert.h`` which defines
``configASSERT`` for you using Pigweed's assert system for your
``FreeRTOSConfig.h`` if you chose to use it.

.. code-block:: cpp

  // Instead of defining configASSERT, simply include this header in its place.
  #include "pw_third_party/freertos/config_assert.h"
