.. _module-pw_sync_baremetal:

-----------------
pw_sync_baremetal
-----------------
This is a set of backends for pw_sync that works on baremetal targets. It is not
ready for use, and is under construction. The provided implementation makes a
single attempt to acquire the lock and asserts if it is unavailable. It does not
perform interrupt masking or disable global interrupts, so this implementation
does not support simultaneous multi-threaded environments including IRQs, and is
only meant to prevent data corruption.
