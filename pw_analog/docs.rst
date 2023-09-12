.. _module-pw_analog:

=========
pw_analog
=========

.. warning::
  This module is under construction and may not be ready for use.

pw_analog contains interfaces and utility functions for using the ADC.

--------
Features
--------

pw::analog::AnalogInput
=======================
The common interface for obtaining ADC samples. This interface represents
a single analog input or channel. Users will need to supply their own ADC
driver implementation in order to configure and enable the ADC peripheral.
Users are responsible for managing multithreaded access to the ADC driver if the
ADC services multiple channels.

pw::analog::MicrovoltInput
==========================
The common interface for obtaining voltage samples in microvolts. This interface
represents a single voltage input or channel. Users will need to supply their
own ADC driver implementation in order to configure and enable the ADC
peripheral in order to provide the reference voltages and to configure and
enable the ADC peripheral where needed. Users are responsible for managing
multithreaded access to the ADC driver if the ADC services multiple channels.

pw::analog::GmockAnalogInput
============================
gMock of AnalogInput used for testing and mocking out the AnalogInput.

pw::analog::GmockMicrovoltInput
===============================
gMock of MicrovoltInput used for testing and mocking out the MicrovoltInput.

-------------
API reference
-------------

pw::analog::AnalogInput
=======================
.. doxygenclass:: pw::analog::AnalogInput
   :members:

pw::analog::GmockAnalogInput
============================
.. literalinclude:: public/pw_analog/analog_input_gmock.h
   :start-after: #pragma once
   :end-before: }  // namespace pw::analog

pw::analog::MicrovoltInput
==========================
.. literalinclude:: public/pw_analog/microvolt_input.h
   :start-after: #pragma once
   :end-before: }  // namespace pw::analog

pw::analog::GmockMicrovoltInput
===============================
.. literalinclude:: public/pw_analog/microvolt_input_gmock.h
   :start-after: #pragma once
   :end-before: }  // namespace pw::analog
