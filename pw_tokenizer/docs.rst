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

Example: tokenize a message with arguments in a custom macro
============================================================
The following example implements a custom tokenization macro similar to
:ref:`module-pw_log_tokenized`.

.. code-block:: cpp

   #include "pw_tokenizer/tokenize.h"

   #ifndef __cplusplus
   extern "C" {
   #endif

   void EncodeTokenizedMessage(uint32_t metadata,
                               pw_tokenizer_Token token,
                               pw_tokenizer_ArgTypes types,
                               ...);

   #ifndef __cplusplus
   }  // extern "C"
   #endif

   #define PW_LOG_TOKENIZED_ENCODE_MESSAGE(metadata, format, ...)         \
     do {                                                                 \
       PW_TOKENIZE_FORMAT_STRING(                                         \
           PW_TOKENIZER_DEFAULT_DOMAIN, UINT32_MAX, format, __VA_ARGS__); \
       EncodeTokenizedMessage(payload,                                    \
                              _pw_tokenizer_token,                        \
                              PW_TOKENIZER_ARG_TYPES(__VA_ARGS__)         \
                                  PW_COMMA_ARGS(__VA_ARGS__));            \
     } while (0)

In this example, the ``EncodeTokenizedMessage`` function would handle encoding
and processing the message. Encoding is done by the
:cpp:class:`pw::tokenizer::EncodedMessage` class or
:cpp:func:`pw::tokenizer::EncodeArgs` function from
``pw_tokenizer/encode_args.h``. The encoded message can then be transmitted or
stored as needed.

.. code-block:: cpp

   #include "pw_log_tokenized/log_tokenized.h"
   #include "pw_tokenizer/encode_args.h"

   void HandleTokenizedMessage(pw::log_tokenized::Metadata metadata,
                               pw::span<std::byte> message);

   extern "C" void EncodeTokenizedMessage(const uint32_t metadata,
                                          const pw_tokenizer_Token token,
                                          const pw_tokenizer_ArgTypes types,
                                          ...) {
     va_list args;
     va_start(args, types);
     pw::tokenizer::EncodedMessage<> encoded_message(token, types, args);
     va_end(args);

     HandleTokenizedMessage(metadata, encoded_message);
   }

.. admonition:: Why use a custom macro

   - Optimal code size. Invoking a free function with the tokenized data results
     in the smallest possible call site.
   - Pass additional arguments, such as metadata, with the tokenized message.
   - Integrate ``pw_tokenizer`` with other systems.

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
