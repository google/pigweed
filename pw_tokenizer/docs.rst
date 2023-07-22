.. _module-pw_tokenizer:

============
pw_tokenizer
============
.. pigweed-module::
   :name: pw_tokenizer
   :tagline: Compress strings to shrink logs by +75%
   :status: stable
   :languages: C11, C++14, Python, TypeScript
   :code-size-impact: 50% reduction in binary log size
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli
      guides: module-pw_tokenizer-guides

Logging is critical, but developers are often forced to choose between
additional logging or saving crucial flash space. The ``pw_tokenizer`` module
helps address this by replacing printf-style strings with binary tokens during
compilation. This enables extensive logging with substantially less memory
usage.

.. note::
  This usage of the term "tokenizer" is not related to parsing! The
  module is called tokenizer because it replaces a whole string literal with an
  integer token. It does not parse strings into separate tokens.

The most common application of ``pw_tokenizer`` is binary logging, and it is
designed to integrate easily into existing logging systems. However, the
tokenizer is general purpose and can be used to tokenize any strings, with or
without printf-style arguments.

**Why tokenize strings?**

* Dramatically reduce binary size by removing string literals from binaries.
* Reduce I/O traffic, RAM, and flash usage by sending and storing compact tokens
  instead of strings. We've seen over 50% reduction in encoded log contents.
* Reduce CPU usage by replacing snprintf calls with simple tokenization code.
* Remove potentially sensitive log, assert, and other strings from binaries.

See :ref:`module-pw_tokenizer-design` for a more detailed explanation
of how ``pw_tokenizer`` works.

.. _module-pw_tokenizer-tokenized-logging-example:

--------------------------
Example: tokenized logging
--------------------------
This example demonstrates using ``pw_tokenizer`` for logging. In this example,
tokenized logging saves ~90% in binary size (41 → 4 bytes) and 70% in encoded
size (49 → 15 bytes).

**Before**: plain text logging

+------------------+-------------------------------------------+---------------+
| Location         | Logging Content                           | Size in bytes |
+==================+===========================================+===============+
| Source contains  | ``LOG("Battery state: %s; battery         |               |
|                  | voltage: %d mV", state, voltage);``       |               |
+------------------+-------------------------------------------+---------------+
| Binary contains  | ``"Battery state: %s; battery             | 41            |
|                  | voltage: %d mV"``                         |               |
+------------------+-------------------------------------------+---------------+
|                  | (log statement is called with             |               |
|                  | ``"CHARGING"`` and ``3989`` as arguments) |               |
+------------------+-------------------------------------------+---------------+
| Device transmits | ``"Battery state: CHARGING; battery       | 49            |
|                  | voltage: 3989 mV"``                       |               |
+------------------+-------------------------------------------+---------------+
| When viewed      | ``"Battery state: CHARGING; battery       |               |
|                  | voltage: 3989 mV"``                       |               |
+------------------+-------------------------------------------+---------------+

**After**: tokenized logging

+------------------+-----------------------------------------------------------+---------+
| Location         | Logging Content                                           | Size in |
|                  |                                                           | bytes   |
+==================+===========================================================+=========+
| Source contains  | ``LOG("Battery state: %s; battery                         |         |
|                  | voltage: %d mV", state, voltage);``                       |         |
+------------------+-----------------------------------------------------------+---------+
| Binary contains  | ``d9 28 47 8e`` (0x8e4728d9)                              | 4       |
+------------------+-----------------------------------------------------------+---------+
|                  | (log statement is called with                             |         |
|                  | ``"CHARGING"`` and ``3989`` as arguments)                 |         |
+------------------+-----------------------------------------------------------+---------+
| Device transmits | =============== ============================== ========== | 15      |
|                  | ``d9 28 47 8e`` ``08 43 48 41 52 47 49 4E 47`` ``aa 3e``  |         |
|                  | --------------- ------------------------------ ---------- |         |
|                  | Token           ``"CHARGING"`` argument        ``3989``,  |         |
|                  |                                                as         |         |
|                  |                                                varint     |         |
|                  | =============== ============================== ========== |         |
+------------------+-----------------------------------------------------------+---------+
| When viewed      | ``"Battery state: CHARGING; battery voltage: 3989 mV"``   |         |
+------------------+-----------------------------------------------------------+---------+

.. toctree::
   :hidden:
   :maxdepth: 1

   api
   cli
   design
   guides
