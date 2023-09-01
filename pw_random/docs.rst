.. _module-pw_random:

=========
pw_random
=========
Pigweed's ``pw_random`` module provides a generic interface for random number
generators, as well as some practical embedded-friendly implementations. While
this module does not provide drivers for hardware random number generators, it
acts as a user-friendly layer that can be used to abstract away such hardware.

Embedded systems have the propensity to be more deterministic than your typical
PC. Sometimes this is a good thing. Other times, it's valuable to have some
random numbers that aren't predictable. In security contexts or areas where
things must be marked with a unique ID, this is especially important. Depending
on the project, true hardware random number generation peripherals may or may
not be available. Even if RNG hardware is present, it might not always be active
or accessible. ``pw_random`` provides libraries that make these situations
easier to manage.

---------------------
Using RandomGenerator
---------------------
There's two sides to a RandomGenerator; the input, and the output. The outputs
are relatively straightforward; ``GetInt(T&)`` randomizes the passed integer
reference, ``GetInt(T&, T exclusive_upper_bound)`` produces a random integer
less than ``exclusive_upper_bound``, and ``Get()`` dumps random values into the
passed span. The inputs are in the form of the ``InjectEntropy*()`` functions.
These functions are used to "seed" the random generator. In some
implementations, this can simply be resetting the seed of a PRNG, while in
others it might directly populate a limited buffer of random data. In all cases,
entropy injection is used to improve the randomness of calls to ``Get*()``.

It might not be easy to find sources of entropy in a system, but in general a
few bits of noise from ADCs or other highly variable inputs can be accumulated
in a RandomGenerator over time to improve randomness. Such an approach might
not be sufficient for security, but it could help for less strict uses.

-------------
API reference
-------------
.. doxygennamespace:: pw::random
   :members:

-----------
Future Work
-----------
A simple "entropy pool" implementation could buffer incoming entropy later use
instead of requiring an application to directly poll the hardware RNG peripheral
when the random data is needed. This would let a device collect entropy when
idling, improving the latency of potentially performance-sensitive areas where
random numbers are needed.
