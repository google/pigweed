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

.. _module-pw_tokenizer-masks:

Smaller tokens with masking
===========================
``pw_tokenizer`` uses 32-bit tokens. On 32-bit or 64-bit architectures, using
fewer than 32 bits does not improve runtime or code size efficiency. However,
when tokens are packed into data structures or stored in arrays, the size of the
token directly affects memory usage. In those cases, every bit counts, and it
may be desireable to use fewer bits for the token.

``pw_tokenizer`` allows users to provide a mask to apply to the token. This
masked token is used in both the token database and the code. The masked token
is not a masked version of the full 32-bit token, the masked token is the token.
This makes it trivial to decode tokens that use fewer than 32 bits.

Masking functionality is provided through the ``*_MASK`` versions of the macros.
For example, the following generates 16-bit tokens and packs them into an
existing value.

.. code-block:: cpp

   constexpr uint32_t token = PW_TOKENIZE_STRING_MASK("domain", 0xFFFF, "Pigweed!");
   uint32_t packed_word = (other_bits << 16) | token;

Tokens are hashes, so tokens of any size have a collision risk. The fewer bits
used for tokens, the more likely two strings are to hash to the same token. See
`token collisions`_.

Masked tokens without arguments may be encoded in fewer bytes. For example, the
16-bit token ``0x1234`` may be encoded as two little-endian bytes (``34 12``)
rather than four (``34 12 00 00``). The detokenizer tools zero-pad data smaller
than four bytes. Tokens with arguments must always be encoded as four bytes.

Token collisions
================
Tokens are calculated with a hash function. It is possible for different
strings to hash to the same token. When this happens, multiple strings will have
the same token in the database, and it may not be possible to unambiguously
decode a token.

The detokenization tools attempt to resolve collisions automatically. Collisions
are resolved based on two things:

- whether the tokenized data matches the strings arguments' (if any), and
- if / when the string was marked as having been removed from the database.

Working with collisions
-----------------------
Collisions may occur occasionally. Run the command
``python -m pw_tokenizer.database report <database>`` to see information about a
token database, including any collisions.

If there are collisions, take the following steps to resolve them.

- Change one of the colliding strings slightly to give it a new token.
- In C (not C++), artificial collisions may occur if strings longer than
  ``PW_TOKENIZER_CFG_C_HASH_LENGTH`` are hashed. If this is happening, consider
  setting ``PW_TOKENIZER_CFG_C_HASH_LENGTH`` to a larger value.  See
  ``pw_tokenizer/public/pw_tokenizer/config.h``.
- Run the ``mark_removed`` command with the latest version of the build
  artifacts to mark missing strings as removed. This deprioritizes them in
  collision resolution.

  .. code-block:: sh

     python -m pw_tokenizer.database mark_removed --database <database> <ELF files>

  The ``purge`` command may be used to delete these tokens from the database.

Probability of collisions
-------------------------
Hashes of any size have a collision risk. The probability of one at least
one collision occurring for a given number of strings is unintuitively high
(this is known as the `birthday problem
<https://en.wikipedia.org/wiki/Birthday_problem>`_). If fewer than 32 bits are
used for tokens, the probability of collisions increases substantially.

This table shows the approximate number of strings that can be hashed to have a
1% or 50% probability of at least one collision (assuming a uniform, random
hash).

+-------+---------------------------------------+
| Token | Collision probability by string count |
| bits  +--------------------+------------------+
|       |         50%        |          1%      |
+=======+====================+==================+
|   32  |       77000        |        9300      |
+-------+--------------------+------------------+
|   31  |       54000        |        6600      |
+-------+--------------------+------------------+
|   24  |        4800        |         580      |
+-------+--------------------+------------------+
|   16  |         300        |          36      |
+-------+--------------------+------------------+
|    8  |          19        |           3      |
+-------+--------------------+------------------+

Keep this table in mind when masking tokens (see `Smaller tokens with
masking`_). 16 bits might be acceptable when tokenizing a small set of strings,
such as module names, but won't be suitable for large sets of strings, like log
messages.

---------------
Token databases
---------------
See :ref:`module-pw_tokenizer-token-databases` for a conceptual overview and
:ref:`module-pw_tokenizer-managing-token-databases` for guides on using token
databases.

.. _module-pw_tokenizer-detokenization:

--------------
Detokenization
--------------
Detokenization is the process of expanding a token to the string it represents
and decoding its arguments. This module provides Python, C++ and TypeScript
detokenization libraries.

**Example: decoding tokenized logs**

A project might tokenize its log messages with the `Base64 format`_. Consider
the following log file, which has four tokenized logs and one plain text log:

.. code-block:: text

   20200229 14:38:58 INF $HL2VHA==
   20200229 14:39:00 DBG $5IhTKg==
   20200229 14:39:20 DBG Crunching numbers to calculate probability of success
   20200229 14:39:21 INF $EgFj8lVVAUI=
   20200229 14:39:23 ERR $DFRDNwlOT1RfUkVBRFk=

The project's log strings are stored in a database like the following:

.. code-block::

   1c95bd1c,          ,"Initiating retrieval process for recovery object"
   2a5388e4,          ,"Determining optimal approach and coordinating vectors"
   3743540c,          ,"Recovery object retrieval failed with status %s"
   f2630112,          ,"Calculated acceptable probability of success (%.2f%%)"

Using the detokenizing tools with the database, the logs can be decoded:

.. code-block:: text

   20200229 14:38:58 INF Initiating retrieval process for recovery object
   20200229 14:39:00 DBG Determining optimal algorithm and coordinating approach vectors
   20200229 14:39:20 DBG Crunching numbers to calculate probability of success
   20200229 14:39:21 INF Calculated acceptable probability of success (32.33%)
   20200229 14:39:23 ERR Recovery object retrieval failed with status NOT_READY

.. note::

   This example uses the `Base64 format`_, which occupies about 4/3 (133%) as
   much space as the default binary format when encoded. For projects that wish
   to interleave tokenized with plain text, using Base64 is a worthwhile
   tradeoff.

Python
======
To detokenize in Python, import ``Detokenizer`` from the ``pw_tokenizer``
package, and instantiate it with paths to token databases or ELF files.

.. code-block:: python

   import pw_tokenizer

   detokenizer = pw_tokenizer.Detokenizer('path/to/database.csv', 'other/path.elf')

   def process_log_message(log_message):
       result = detokenizer.detokenize(log_message.payload)
       self._log(str(result))

The ``pw_tokenizer`` package also provides the ``AutoUpdatingDetokenizer``
class, which can be used in place of the standard ``Detokenizer``. This class
monitors database files for changes and automatically reloads them when they
change. This is helpful for long-running tools that use detokenization. The
class also supports token domains for the given database files in the
``<path>#<domain>`` format.

For messages that are optionally tokenized and may be encoded as binary,
Base64, or plaintext UTF-8, use
:func:`pw_tokenizer.proto.decode_optionally_tokenized`. This will attempt to
determine the correct method to detokenize and always provide a printable
string. For more information on this feature, see
:ref:`module-pw_tokenizer-proto`.


C++
===
The C++ detokenization libraries can be used in C++ or any language that can
call into C++ with a C-linkage wrapper, such as Java or Rust. A reference
Java Native Interface (JNI) implementation is provided.

The C++ detokenization library uses binary-format token databases (created with
``database.py create --type binary``). Read a binary format database from a
file or include it in the source code. Pass the database array to
``TokenDatabase::Create``, and construct a detokenizer.

.. code-block:: cpp

   Detokenizer detokenizer(TokenDatabase::Create(token_database_array));

   std::string ProcessLog(span<uint8_t> log_data) {
     return detokenizer.Detokenize(log_data).BestString();
   }

The ``TokenDatabase`` class verifies that its data is valid before using it. If
it is invalid, the ``TokenDatabase::Create`` returns an empty database for which
``ok()`` returns false. If the token database is included in the source code,
this check can be done at compile time.

.. code-block:: cpp

   // This line fails to compile with a static_assert if the database is invalid.
   constexpr TokenDatabase kDefaultDatabase =  TokenDatabase::Create<kData>();

   Detokenizer OpenDatabase(std::string_view path) {
     std::vector<uint8_t> data = ReadWholeFile(path);

     TokenDatabase database = TokenDatabase::Create(data);

     // This checks if the file contained a valid database. It is safe to use a
     // TokenDatabase that failed to load (it will be empty), but it may be
     // desirable to provide a default database or otherwise handle the error.
     if (database.ok()) {
       return Detokenizer(database);
     }
     return Detokenizer(kDefaultDatabase);
   }


TypeScript
==========
To detokenize in TypeScript, import ``Detokenizer`` from the ``pigweedjs``
package, and instantiate it with a CSV token database.

.. code-block:: typescript

   import { pw_tokenizer, pw_hdlc } from 'pigweedjs';
   const { Detokenizer } = pw_tokenizer;
   const { Frame } = pw_hdlc;

   const detokenizer = new Detokenizer(String(tokenCsv));

   function processLog(frame: Frame){
     const result = detokenizer.detokenize(frame);
     console.log(result);
   }

For messages that are encoded in Base64, use ``Detokenizer::detokenizeBase64``.
`detokenizeBase64` will also attempt to detokenize nested Base64 tokens. There
is also `detokenizeUint8Array` that works just like `detokenize` but expects
`Uint8Array` instead of a `Frame` argument.

Protocol buffers
================
``pw_tokenizer`` provides utilities for handling tokenized fields in protobufs.
See :ref:`module-pw_tokenizer-proto` for details.

.. toctree::
   :hidden:

   proto.rst

.. _module-pw_tokenizer-base64-format:

-------------
Base64 format
-------------
The tokenizer encodes messages to a compact binary representation. Applications
may desire a textual representation of tokenized strings. This makes it easy to
use tokenized messages alongside plain text messages, but comes at a small
efficiency cost: encoded Base64 messages occupy about 4/3 (133%) as much memory
as binary messages.

The Base64 format is comprised of a ``$`` character followed by the
Base64-encoded contents of the tokenized message. For example, consider
tokenizing the string ``This is an example: %d!`` with the argument -1. The
string's token is 0x4b016e66.

.. code-block:: text

   Source code: PW_LOG("This is an example: %d!", -1);

    Plain text: This is an example: -1! [23 bytes]

        Binary: 66 6e 01 4b 01          [ 5 bytes]

        Base64: $Zm4BSwE=               [ 9 bytes]

Encoding
========
To encode with the Base64 format, add a call to
``pw::tokenizer::PrefixedBase64Encode`` or ``pw_tokenizer_PrefixedBase64Encode``
in the tokenizer handler function. For example,

.. code-block:: cpp

   void TokenizedMessageHandler(const uint8_t encoded_message[],
                                size_t size_bytes) {
     pw::InlineBasicString base64 = pw::tokenizer::PrefixedBase64Encode(
         pw::span(encoded_message, size_bytes));

     TransmitLogMessage(base64.data(), base64.size());
   }

Decoding
========
The Python ``Detokenizer`` class supports decoding and detokenizing prefixed
Base64 messages with ``detokenize_base64`` and related methods.

.. tip::
   The Python detokenization tools support recursive detokenization for prefixed
   Base64 text. Tokenized strings found in detokenized text are detokenized, so
   prefixed Base64 messages can be passed as ``%s`` arguments.

   For example, the tokenized string for "Wow!" is ``$RhYjmQ==``. This could be
   passed as an argument to the printf-style string ``Nested message: %s``, which
   encodes to ``$pEVTYQkkUmhZam1RPT0=``. The detokenizer would decode the message
   as follows:

   ::

     "$pEVTYQkkUmhZam1RPT0=" → "Nested message: $RhYjmQ==" → "Nested message: Wow!"

Base64 decoding is supported in C++ or C with the
``pw::tokenizer::PrefixedBase64Decode`` or ``pw_tokenizer_PrefixedBase64Decode``
functions.

Investigating undecoded messages
================================
Tokenized messages cannot be decoded if the token is not recognized. The Python
package includes the ``parse_message`` tool, which parses tokenized Base64
messages without looking up the token in a database. This tool attempts to guess
the types of the arguments and displays potential ways to decode them.

This tool can be used to extract argument information from an otherwise unusable
message. It could help identify which statement in the code produced the
message. This tool is not particularly helpful for tokenized messages without
arguments, since all it can do is show the value of the unknown token.

The tool is executed by passing Base64 tokenized messages, with or without the
``$`` prefix, to ``pw_tokenizer.parse_message``. Pass ``-h`` or ``--help`` to
see full usage information.

Example
-------
.. code-block::

   $ python -m pw_tokenizer.parse_message '$329JMwA=' koSl524TRkFJTEVEX1BSRUNPTkRJVElPTgJPSw== --specs %s %d

   INF Decoding arguments for '$329JMwA='
   INF Binary: b'\xdfoI3\x00' [df 6f 49 33 00] (5 bytes)
   INF Token:  0x33496fdf
   INF Args:   b'\x00' [00] (1 bytes)
   INF Decoding with up to 8 %s or %d arguments
   INF   Attempt 1: [%s]
   INF   Attempt 2: [%d] 0

   INF Decoding arguments for '$koSl524TRkFJTEVEX1BSRUNPTkRJVElPTgJPSw=='
   INF Binary: b'\x92\x84\xa5\xe7n\x13FAILED_PRECONDITION\x02OK' [92 84 a5 e7 6e 13 46 41 49 4c 45 44 5f 50 52 45 43 4f 4e 44 49 54 49 4f 4e 02 4f 4b] (28 bytes)
   INF Token:  0xe7a58492
   INF Args:   b'n\x13FAILED_PRECONDITION\x02OK' [6e 13 46 41 49 4c 45 44 5f 50 52 45 43 4f 4e 44 49 54 49 4f 4e 02 4f 4b] (24 bytes)
   INF Decoding with up to 8 %s or %d arguments
   INF   Attempt 1: [%d %s %d %d %d] 55 FAILED_PRECONDITION 1 -40 -38
   INF   Attempt 2: [%d %s %s] 55 FAILED_PRECONDITION OK

Detokenizing command line utilities
-----------------------------------
See :ref:`module-pw_tokenizer-cli-detokenizing`.

.. toctree::
   :hidden:
   :maxdepth: 1

   api
   cli
   design
   guides
