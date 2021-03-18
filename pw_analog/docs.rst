.. _module-pw_analog:

---------
pw_analog
---------

.. warning::
  This module is under construction and may not be ready for use.

pw_analog contains interfaces and utility functions for using the ADC.

Features
========

pw::analog::AnalogInput
-----------------------
The common interface for obtaining ADC samples. This interface represents
a single analog input or channel. Users will need to supply their own ADC
driver implementation in order to configure and enable the ADC peripheral.
Users are responsible for managing multithreaded access to the ADC driver if the
ADC services multiple channels.
