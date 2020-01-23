.. _chapter-pw-assert:

.. default-domain:: cpp

.. highlight:: cpp

---------
pw_assert
---------
Pigweed's assert module provides facilities for applications to check
preconditions.  The module is split into two components:

1. The facade (this module) which is only a macro interface layer
2. The backend, provided elsewhere, that implements the low level checks

Basic usage example
-------------------

.. code-block:: cpp

  #include "pw_assert/assert.h"

  int main() {
    bool sensor_running = StartSensor(&msg);
    PW_CHECK(sensor_running, "Sensor failed to start; code: %s", msg);
  }

