.. _module-pw_sync_baremetal:

=================
pw_sync_baremetal
=================
This is a set of backends for pw_sync that works on baremetal targets. It is not
ready for use, and is under construction.

.. note::
  All constructs in this baremetal backend do not support hardware multi-threading
  (SMP, SMT, etc).

.. warning::
  It does not perform interrupt masking or disable global interrupts. This is not
  safe to use yet!

-------------------------------------
pw_sync_baremetal's InterruptSpinLock
-------------------------------------
The interrupt spin-lock implementation makes a single attempt to acquire the lock
and asserts if it is unavailable. It does not perform interrupt masking or disable global
interrupts.

-------------------------
pw_sync_baremetal's Mutex
-------------------------
The mutex implementation makes a single attempt to acquire the lock and asserts if
it is unavailable.
