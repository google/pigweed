.. _module-pw_log_fuchsia:

==============
pw_log_fuchsia
==============
.. pigweed-module::
   :name: pw_log_fuchsia

--------
Overview
--------
The ``pw_log_fuchsia`` module provides a ``PW_HANDLE_LOG`` backend for the
``pw_log`` module. The backend uses the ``fuchsia.logger.LogSink`` FIDL API
to send logs. Only Bazel is supported because the Fuchsia SDK only supports Bazel.

---
API
---
Before logging, ``pw::log_fuchsia::InitializeLogging(async_dispatcher_t*)`` must
be called to initialize the logging state.

Two log flags are supported to help with logging during tests:
``PW_LOG_FLAG_USE_PRINTF``, which uses printf to log instead of ``LogSink``,
and ``PW_LOG_FLAG_IGNORE``, which skips logging the log.
