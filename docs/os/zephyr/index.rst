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

.. _docs-os-zephyr-get-started:

-----------------------------------
Get started with Zephyr and Pigweed
-----------------------------------
1. Check out the `zephyr_pigweed`_ repository for an example of a Zephyr starter
   project that has been set up to use Pigweed.
2. See :ref:`docs-os-zephyr-kconfig` to find the Kconfig options for
   enabling individual Pigweed modules and features.

-------
Testing
-------
To test against Zephyr, first go through the `zephyr_pigweed`_ tutorial.
Once set up, simply invoke:

.. code-block:: bash

   $ source ${PW_ROOT}/activate.sh
   $ west twister -T ${PW_ROOT}

.. attention:: Testing has only been verified with `-p native_posix`. Proceed with caution.

.. _zephyr_pigweed: https://github.com/yperess/zephyr-pigweed/

.. toctree::
   :hidden:

   kconfig
