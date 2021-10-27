.. _module-pw_assert_zephyr:

================
pw_assert_zephyr
================

--------
Overview
--------
This assert backend implements the ``pw_assert`` facade, by routing the assert
message to the Zephyr assert subsystem. Failed asserts will call ``k_panic()``.
To enable the assert module, set ``CONFIG_PIGWEED_ASSERT=y``. After that,
Zephyr's assert configs can be used to control the behavior via CONFIG_ASSERT_
and CONFIG_ASSERT_LEVEL_.

.. _CONFIG_ASSERT: https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_ASSERT.html#std-kconfig-CONFIG_ASSERT
.. _CONFIG_ASSERT_LEVEL: https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_ASSERT_LEVEL.html#std-kconfig-CONFIG_ASSERT_LEVEL