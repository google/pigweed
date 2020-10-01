.. _module-pw_assert_log:

=============
pw_assert_log
=============

--------
Overview
--------
This assert backend implements the ``pw_assert`` facade, by routing the assert
message into the logger with the ``PW_LOG_ASSERT_FAILED`` flag set. This is an
easy way to tokenize your assert messages, by using the ``pw_log_tokenized``
log backend for logging, then using ``pw_assert_log`` to route the tokenized
messages into the tokenized log handler.

To use this module:

1. Set your assert backend: ``pw_assert_BACKEND = dir_pw_assert_log``
2. Ensure your logging backend knows how to handle the assert failure flag
