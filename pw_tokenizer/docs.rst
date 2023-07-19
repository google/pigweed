.. _module-pw_tokenizer:

============
pw_tokenizer
============
.. pigweed-module::
   :name: pw_tokenizer
   :tagline: Cut your log sizes in half
   :status: stable
   :languages: C11, C++14, Python, TypeScript
   :code-size-impact: 50% reduction in binary log size
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli

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
of how ``pw__tokenizer`` works and :ref:`module-pw_tokenizer-design-example`
for an example of how much ``pw_tokenizer`` can save you in binary size.

---------------
Getting started
---------------
See :ref:`module-pw_tokenizer-get-started`.

------------
Tokenization
------------
See :ref:`module-pw_tokenizer-api-tokenization` in the API reference
for detailed information about the tokenization API.

Binary logging with pw_tokenizer
================================
String tokenization can be used to convert plain text logs to a compact,
efficient binary format. See :ref:`module-pw_log_tokenized`.

Encoding command line utility
=============================
See :ref:`module-pw_tokenizer-cli-encoding`.

Tokenization domains
====================
See :ref:`module-pw_tokenizer-domains`.

Masking
=======
See :ref:`module-pw_tokenizer-masks`.

Token collisions
================
See :ref:`module-pw_tokenizer-collisions` for a conceptual overview and
:ref:`module-pw_tokenizer-collisions-guide` for guidance on how to fix
collisions.

---------------
Token databases
---------------
See :ref:`module-pw_tokenizer-token-databases` for a conceptual overview and
:ref:`module-pw_tokenizer-managing-token-databases` for guides on using token
databases.

--------------
Detokenization
--------------
See :ref:`module-pw_tokenizer-detokenization` for a conceptual overview of
detokenization and :ref:`module-pw_tokenizer-detokenization-guides` for detailed
instructions on how to do detokenization in different programming languages.

-------------
Base64 format
-------------
See :ref:`module-pw_tokenizer-base64-format` for a conceptual overview and
:ref:`module-pw_tokenizer-base64-guides` for usage.

.. toctree::
   :hidden:
   :maxdepth: 1

   api
   cli
   design
   guides
   proto
