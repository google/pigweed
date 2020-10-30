.. _module-pw_chrono:

---------
pw_chrono
---------
Pigweed's chrono module provides facilities for applications to deal with time,
leveraging many pieces of STL's the ``std::chrono`` library but with a focus
on portability for constrained embedded devices and maintaining correctness.

.. warning::
  This module is under construction, not ready for use, and the documentation
  is incomplete.

SystemClock facade
------------------
The ``pw::chrono::SystemClock`` is meant to serve as the clock used for time
bound operations such as thread sleeping, waiting on mutexes/semaphores, etc.
The ``SystemClock`` always uses a signed 64 bit as the underlying type for time
points and durations. This means users do not have to worry about clock overflow
risk as long as rational durations and time points as used, i.e. within a range
of ±292 years.
