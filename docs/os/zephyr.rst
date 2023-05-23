.. _docs-os-zephyr:

======
Zephyr
======
Pigweed has preliminary support for `Zephyr <https://www.zephyrproject.org/>`_.
See the docs for these modules for more information:

- :ref:`pw_assert_zephyr <module-pw_assert_zephyr>`
- :ref:`pw_chrono_zephyr <module-pw_chrono_zephyr>`
- :ref:`pw_interrupt_zephyr <module-pw_interrupt_zephyr>`
- :ref:`pw_log_zephyr <module-pw_log_zephyr>`
- :ref:`pw_sync_zephyr <module-pw_sync_zephyr>`
- :ref:`pw_sys_io_zephyr <module-pw_sys_io_zephyr>`
- :ref:`pw_thread_zephyr <module-pw_thread_zephyr>`

-------
Testing
-------
To test against Zephyr, first go through the `Zephyr Getting Started Guide`_.
Once set up, simply invoke:

.. code-block:: bash

   $ . ${PW_ROOT}/activate.sh
   $ ${ZEPHYR_BASE}/scripts/twister -T ${PW_ROOT}

.. attention:: Testing has only been verified with `-p native_posix`. Proceed with caution.

.. _Zephyr Getting Started Guide: https://docs.zephyrproject.org/latest/develop/getting_started/index.html#getting-started-guide
