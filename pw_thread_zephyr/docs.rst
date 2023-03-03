.. _module-pw_thread_zephyr:

----------------
pw_thread_zephyr
----------------
This is a set of backends for pw_thread based on the Zephyr RTOS. Currently,
only the pw_thread.sleep facade is implemented which is enabled via
``CONFIG_PIGWEED_THREAD_SLEEP=y``.
