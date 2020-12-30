.. _module-pw_sync_threadx:

---------------
pw_sync_threadx
---------------
This is a set of backends for pw_sync based on ThreadX. It is not ready for use,
and is under construction.

It is possible, if necessary, to use pw_sync_threadx without using the Pigweed
provided pw_chrono_threadx in case the ThreadX time API (``tx_time_get()``)) is
not available (i.e. ``TX_NO_TIMER`` is set). You are responsible for ensuring
that the chrono backend provided has counts which match the ThreadX tick based
API.
