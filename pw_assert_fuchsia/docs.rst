.. _module-pw_assert_fuchsia:

=================
pw_assert_fuchsia
=================
.. pigweed-module::
   :name: pw_assert_fuchsia

--------
Overview
--------
The ``pw_assert_fuchsia`` module provides ``PW_ASSERT()`` and ``PW_CHECK_*()``
backends for the ``pw_assert`` module that call ``ZX_PANIC()`` on Fuchsia. Only Bazel
is supported because the Fuchsia SDK only supports Bazel.
