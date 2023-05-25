.. _module-pw_interrupt:

------------
pw_interrupt
------------
Pigweed's interrupt module provides a consistent interface for determining
whether your code is currently executing in an interrupt context (IRQ or NMI)
or not.

.. doxygenfunction:: pw::interrupt::InInterruptContext()
