.. _module-pw_chrono:

---------
pw_chrono
---------
Pigweed's chrono module provides facilities for applications to deal with time,
leveraging many pieces of STL's the ``std::chrono`` library but with a focus
on portability for constrained embedded devices and maintaining correctness.

At a high level Pigweed's time primitives rely on C++'s
`<chrono> <https://en.cppreference.com/w/cpp/header/chrono>`_ library to enable
users to express intents with strongly typed real time units. In addition, it
extends the C++ named
`Clock <https://en.cppreference.com/w/cpp/named_req/Clock>`_ and
`TrivialClock <https://en.cppreference.com/w/cpp/named_req/TrivialClock>`_
requirements with additional attributes such as whether a clock is monotonic
(not just steady), is always enabled (or requires enabling), is free running
(works even if interrupts are masked), whether it is safe to use in a
Non-Maskable Interrupts (NMI), what the epoch is, and more.

.. warning::
  This module is still under construction, the API is not yet stable. Also the
  documentation is incomplete.

SystemClock facade
------------------
The ``pw::chrono::SystemClock`` is meant to serve as the clock used for time
bound operations such as thread sleeping, waiting on mutexes/semaphores, etc.
The ``SystemClock`` always uses a signed 64 bit as the underlying type for time
points and durations. This means users do not have to worry about clock overflow
risk as long as rational durations and time points as used, i.e. within a range
of ±292 years.
