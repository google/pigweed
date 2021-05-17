.. _module-pw_multisink:

============
pw_multisink
============
This is an module that forwards messages to multiple attached sinks, which
consume messages asynchronously. It is not ready for use and is under
construction.

Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration
of this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_MULTISINK_CONFIG_LOCK_INTERRUPT_SAFE

  Whether an interrupt-safe lock is used to guard multisink read/write operations.

  By default, this option is enabled and the multisink uses an interrupt spin-lock
  to guard its transactions. If disabled, a mutex is used instead.

  Disabling this will alter the entry precondition of the multisink, requiring that
  it not be called from an interrupt context.
