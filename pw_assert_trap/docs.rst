.. _module-pw_assert_trap:

===============
pw_assert_trap
===============
.. pigweed-module::
   :name: pw_assert_trap

--------
Overview
--------
The ``pw_assert_trap`` module provides ``PW_ASSERT()`` and ``PW_CHECK_*()``
backends for the ``pw_assert`` module. This backend will call
``__builtin_trap()`` on assert.

..
   TODO: https://pwbug.dev/351886600 - add docs
