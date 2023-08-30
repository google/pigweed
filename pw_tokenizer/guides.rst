.. _module-pw_tokenizer-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Compress strings to shrink logs by +75%

.. _module-pw_tokenizer-get-started:

---------------
Getting started
---------------
There are two sides to ``pw_tokenizer``, which we call tokenization and
detokenization.

* **Tokenization** converts string literals in the source code to binary tokens
  at compile time. If the string has printf-style arguments, these are encoded
  to compact binary form at runtime.
* **Detokenization** converts tokenized strings back to the original
  human-readable strings.

Here's an overview of what happens when ``pw_tokenizer`` is used:

1. During compilation, the ``pw_tokenizer`` module hashes string literals to
   generate stable 32-bit tokens.
2. The tokenization macro removes these strings by declaring them in an ELF
   section that is excluded from the final binary.
3. After compilation, strings are extracted from the ELF to build a database of
   tokenized strings for use by the detokenizer. The ELF file may also be used
   directly.
4. During operation, the device encodes the string token and its arguments, if
   any.
5. The encoded tokenized strings are sent off-device or stored.
6. Off-device, the detokenizer tools use the token database to decode the
   strings to human-readable form.

Integrating with Bazel / GN / CMake projects
============================================
Integrating ``pw_tokenizer`` requires a few steps beyond building the code. This
section describes one way ``pw_tokenizer`` might be integrated with a project.
These steps can be adapted as needed.

#. Add ``pw_tokenizer`` to your build. Build files for GN, CMake, and Bazel are
   provided. For Make or other build systems, add the files specified in the
   BUILD.gn's ``pw_tokenizer`` target to the build.
#. Use the tokenization macros in your code. See
   :ref:`module-pw_tokenizer-tokenization-guides`.
#. Add the contents of ``pw_tokenizer_linker_sections.ld`` to your project's
   linker script. In GN and CMake, this step is done automatically.
#. Compile your code to produce an ELF file.
#. Run ``database.py create`` on the ELF file to generate a CSV token
   database. See :ref:`module-pw_tokenizer-managing-token-databases`.
#. Commit the token database to your repository. See notes in
   :ref:`module-pw_tokenizer-database-management`.
#. Integrate a ``database.py add`` command to your build to automatically update
   the committed token database. In GN, use the ``pw_tokenizer_database``
   template to do this. See :ref:`module-pw_tokenizer-update-token-database`.
#. Integrate ``detokenize.py`` or the C++ detokenization library with your tools
   to decode tokenized logs. See :ref:`module-pw_tokenizer-detokenization`.

Using with Zephyr
=================
When building ``pw_tokenizer`` with Zephyr, 3 Kconfigs can be used currently:

* ``CONFIG_PIGWEED_TOKENIZER`` will automatically link ``pw_tokenizer`` as well
  as any dependencies.
* ``CONFIG_PIGWEED_TOKENIZER_BASE64`` will automatically link
  ``pw_tokenizer.base64`` as well as any dependencies.
* ``CONFIG_PIGWEED_DETOKENIZER`` will automatically link
  ``pw_tokenizer.decoder`` as well as any dependencies.

Once enabled, the tokenizer headers can be included like any Zephyr headers:

.. code-block:: cpp

   #include <pw_tokenizer/tokenize.h>

.. note::
  Zephyr handles the additional linker sections via
  ``pw_tokenizer_zephyr.ld`` which is added to the end of the linker file
  via a call to ``zephyr_linker_sources(SECTIONS ...)``.

.. _module-pw_tokenizer-tokenization-guides:

------------
Tokenization
------------
Tokenization converts a string literal to a token. If it's a printf-style
string, its arguments are encoded along with it. The results of tokenization can
be sent off device or stored in place of a full string.

.. Note: pw_tokenizer_Token is a C typedef so you would expect to reference it
.. as :c:type:`pw_tokenizer_Token`. That doesn't work because it's defined in
.. a header file that mixes C and C++.

* :cpp:type:`pw_tokenizer_Token`

To tokenize a string, include ``pw_tokenizer/tokenize.h`` and invoke one of the
``PW_TOKENIZE_*`` macros.

Tokenize string literals outside of expressions
===============================================
``pw_tokenizer`` provides macros for tokenizing string literals with no
arguments:

* :c:macro:`PW_TOKENIZE_STRING`
* :c:macro:`PW_TOKENIZE_STRING_DOMAIN`
* :c:macro:`PW_TOKENIZE_STRING_MASK`

The tokenization macros above cannot be used inside other expressions.

.. admonition:: **Yes**: Assign :c:macro:`PW_TOKENIZE_STRING` to a ``constexpr`` variable.
  :class: checkmark

  .. code-block:: cpp

    constexpr uint32_t kGlobalToken = PW_TOKENIZE_STRING("Wowee Zowee!");

    void Function() {
      constexpr uint32_t local_token = PW_TOKENIZE_STRING("Wowee Zowee?");
    }

.. admonition:: **No**: Use :c:macro:`PW_TOKENIZE_STRING` in another expression.
  :class: error

  .. code-block:: cpp

   void BadExample() {
     ProcessToken(PW_TOKENIZE_STRING("This won't compile!"));
   }

  Use :c:macro:`PW_TOKENIZE_STRING_EXPR` instead.

Tokenize inside expressions
===========================
An alternate set of macros are provided for use inside expressions. These make
use of lambda functions, so while they can be used inside expressions, they
require C++ and cannot be assigned to constexpr variables or be used with
special function variables like ``__func__``.

* :c:macro:`PW_TOKENIZE_STRING_EXPR`
* :c:macro:`PW_TOKENIZE_STRING_DOMAIN_EXPR`
* :c:macro:`PW_TOKENIZE_STRING_MASK_EXPR`

.. admonition:: When to use these macros

  Use :c:macro:`PW_TOKENIZE_STRING` and related macros to tokenize string
  literals that do not need %-style arguments encoded.

.. admonition:: **Yes**: Use :c:macro:`PW_TOKENIZE_STRING_EXPR` within other expressions.
  :class: checkmark

  .. code-block:: cpp

    void GoodExample() {
      ProcessToken(PW_TOKENIZE_STRING_EXPR("This will compile!"));
    }

.. admonition:: **No**: Assign :c:macro:`PW_TOKENIZE_STRING_EXPR` to a ``constexpr`` variable.
  :class: error

  .. code-block:: cpp

     constexpr uint32_t wont_work = PW_TOKENIZE_STRING_EXPR("This won't compile!"));

  Instead, use :c:macro:`PW_TOKENIZE_STRING` to assign to a ``constexpr`` variable.

.. admonition:: **No**: Tokenize ``__func__`` in :c:macro:`PW_TOKENIZE_STRING_EXPR`.
  :class: error

  .. code-block:: cpp

    void BadExample() {
      // This compiles, but __func__ will not be the outer function's name, and
      // there may be compiler warnings.
      constexpr uint32_t wont_work = PW_TOKENIZE_STRING_EXPR(__func__);
    }

  Instead, use :c:macro:`PW_TOKENIZE_STRING` to tokenize ``__func__`` or similar macros.

Tokenize a message with arguments to a buffer
=============================================
* :c:macro:`PW_TOKENIZE_TO_BUFFER`
* :c:macro:`PW_TOKENIZE_TO_BUFFER_DOMAIN`
* :c:macro:`PW_TOKENIZE_TO_BUFFER_MASK`

.. admonition:: Why use this macro

   - Encode a tokenized message for consumption within a function.
   - Encode a tokenized message into an existing buffer.

   Avoid using ``PW_TOKENIZE_TO_BUFFER`` in widely expanded macros, such as a
   logging macro, because it will result in larger code size than passing the
   tokenized data to a function.

.. _module-pw_tokenizer-custom-macro:

Tokenize a message with arguments in a custom macro
===================================================
Projects can leverage the tokenization machinery in whichever way best suits
their needs. The most efficient way to use ``pw_tokenizer`` is to pass tokenized
data to a global handler function. A project's custom tokenization macro can
handle tokenized data in a function of their choosing.

``pw_tokenizer`` provides two low-level macros for projects to use
to create custom tokenization macros:

* :c:macro:`PW_TOKENIZE_FORMAT_STRING`
* :c:macro:`PW_TOKENIZER_ARG_TYPES`

.. caution::

   Note the spelling difference! The first macro begins with ``PW_TOKENIZE_``
   (no ``R``) whereas the second begins with ``PW_TOKENIZER`` (``R`` included).

The outputs of these macros are typically passed to an encoding function. That
function encodes the token, argument types, and argument data to a buffer using
helpers provided by ``pw_tokenizer/encode_args.h``:

.. Note: pw_tokenizer_EncodeArgs is a C function so you would expect to
.. reference it as :c:func:`pw_tokenizer_EncodeArgs`. That doesn't work because
.. it's defined in a header file that mixes C and C++.

* :cpp:func:`pw::tokenizer::EncodeArgs`
* :cpp:class:`pw::tokenizer::EncodedMessage`
* :cpp:func:`pw_tokenizer_EncodeArgs`

Example
-------

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
     pw::tokenizer::EncodedMessage<kLogBufferSize> encoded_message(token, types, args);
     va_end(args);

     HandleTokenizedMessage(metadata, encoded_message);
   }

.. admonition:: Why use a custom macro

   - Optimal code size. Invoking a free function with the tokenized data results
     in the smallest possible call site.
   - Pass additional arguments, such as metadata, with the tokenized message.
   - Integrate ``pw_tokenizer`` with other systems.

Tokenizing function names
=========================
The string literal tokenization functions support tokenizing string literals or
constexpr character arrays (``constexpr const char[]``). In GCC and Clang, the
special ``__func__`` variable and ``__PRETTY_FUNCTION__`` extension are declared
as ``static constexpr char[]`` in C++ instead of the standard ``static const
char[]``. This means that ``__func__`` and ``__PRETTY_FUNCTION__`` can be
tokenized while compiling C++ with GCC or Clang.

.. code-block:: cpp

   // Tokenize the special function name variables.
   constexpr uint32_t function = PW_TOKENIZE_STRING(__func__);
   constexpr uint32_t pretty_function = PW_TOKENIZE_STRING(__PRETTY_FUNCTION__);

Note that ``__func__`` and ``__PRETTY_FUNCTION__`` are not string literals.
They are defined as static character arrays, so they cannot be implicitly
concatentated with string literals. For example, ``printf(__func__ ": %d",
123);`` will not compile.

Calculate minimum required buffer size
======================================
* :cpp:func:`pw::tokenizer::MinEncodingBufferSizeBytes`

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

Masking functionality is provided through the ``*_MASK`` versions of the macros:

* :c:macro:`PW_TOKENIZE_STRING_MASK`
* :c:macro:`PW_TOKENIZE_STRING_MASK_EXPR`
* :c:macro:`PW_TOKENIZE_TO_BUFFER_MASK`

For example, the following generates 16-bit tokens and packs them into an
existing value.

.. code-block:: cpp

   constexpr uint32_t token = PW_TOKENIZE_STRING_MASK("domain", 0xFFFF, "Pigweed!");
   uint32_t packed_word = (other_bits << 16) | token;

Tokens are hashes, so tokens of any size have a collision risk. The fewer bits
used for tokens, the more likely two strings are to hash to the same token. See
:ref:`module-pw_tokenizer-collisions`.

Masked tokens without arguments may be encoded in fewer bytes. For example, the
16-bit token ``0x1234`` may be encoded as two little-endian bytes (``34 12``)
rather than four (``34 12 00 00``). The detokenizer tools zero-pad data smaller
than four bytes. Tokens with arguments must always be encoded as four bytes.

.. _module-pw_tokenizer-base64-encoding:

Encoding Base64
===============
See :ref:`module-pw_tokenizer-base64-format` for a conceptual overview of
Base64.

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

Tokenization in Python
======================
The Python ``pw_tokenizer.encode`` module has limited support for encoding
tokenized messages with the ``encode_token_and_args`` function.

.. autofunction:: pw_tokenizer.encode.encode_token_and_args

This function requires a string's token is already calculated. Typically these
tokens are provided by a database, but they can be manually created using the
tokenizer hash.

.. autofunction:: pw_tokenizer.tokens.pw_tokenizer_65599_hash

This is particularly useful for offline token database generation in cases where
tokenized strings in a binary cannot be embedded as parsable pw_tokenizer
entries.

.. note::
   In C, the hash length of a string has a fixed limit controlled by
   ``PW_TOKENIZER_CFG_C_HASH_LENGTH``. To match tokens produced by C (as opposed
   to C++) code, ``pw_tokenizer_65599_hash()`` should be called with a matching
   hash length limit. When creating an offline database, it's a good idea to
   generate tokens for both, and merge the databases.

.. _module-pw_tokenizer-protobuf-tokenization-python:

Protobuf detokenization library
-------------------------------
The :py:mod:`pw_tokenizer.proto` Python module defines functions that may be
used to detokenize protobuf objects in Python. The function
:py:func:`pw_tokenizer.proto.detokenize_fields` detokenizes all fields
annotated as tokenized, replacing them with their detokenized version. For
example:

.. code-block:: python

   my_detokenizer = pw_tokenizer.Detokenizer(some_database)

   my_message = SomeMessage(tokenized_field=b'$YS1EMQ==')
   pw_tokenizer.proto.detokenize_fields(my_detokenizer, my_message)

   assert my_message.tokenized_field == b'The detokenized string! Cool!'

.. _module-pw_tokenizer-managing-token-databases:

---------------
Token databases
---------------
Background: :ref:`module-pw_tokenizer-token-databases`

Token databases are managed with the ``database.py`` script. This script can be
used to extract tokens from compilation artifacts and manage database files.
Invoke ``database.py`` with ``-h`` for full usage information.

An example ELF file with tokenized logs is provided at
``pw_tokenizer/py/example_binary_with_tokenized_strings.elf``. You can use that
file to experiment with the ``database.py`` commands.

.. _module-pw_tokenizer-database-creation:

Create a database
=================
The ``create`` command makes a new token database from ELF files (.elf, .o, .so,
etc.), archives (.a), existing token databases (CSV or binary), or a JSON file
containing an array of strings.

.. code-block:: sh

   ./database.py create --database DATABASE_NAME ELF_OR_DATABASE_FILE...

Two database output formats are supported: CSV and binary. Provide
``--type binary`` to ``create`` to generate a binary database instead of the
default CSV. CSV databases are great for checking into a source control or for
human review. Binary databases are more compact and simpler to parse. The C++
detokenizer library only supports binary databases currently.

.. _module-pw_tokenizer-update-token-database:

Update a database
=================
As new tokenized strings are added, update the database with the ``add``
command.

.. code-block:: sh

   ./database.py add --database DATABASE_NAME ELF_OR_DATABASE_FILE...

This command adds new tokens from ELF files or other databases to the database.
Adding tokens already present in the database updates the date removed, if any,
to the latest.

A CSV token database can be checked into a source repository and updated as code
changes are made. The build system can invoke ``database.py`` to update the
database after each build.

GN integration
==============
Token databases may be updated or created as part of a GN build. The
``pw_tokenizer_database`` template provided by
``$dir_pw_tokenizer/database.gni`` automatically updates an in-source tokenized
strings database or creates a new database with artifacts from one or more GN
targets or other database files.

To create a new database, set the ``create`` variable to the desired database
type (``"csv"`` or ``"binary"``). The database will be created in the output
directory. To update an existing database, provide the path to the database with
the ``database`` variable.

.. code-block::

   import("//build_overrides/pigweed.gni")

   import("$dir_pw_tokenizer/database.gni")

   pw_tokenizer_database("my_database") {
     database = "database_in_the_source_tree.csv"
     targets = [ "//firmware/image:foo(//targets/my_board:some_toolchain)" ]
     input_databases = [ "other_database.csv" ]
   }

Instead of specifying GN targets, paths or globs to output files may be provided
with the ``paths`` option.

.. code-block::

   pw_tokenizer_database("my_database") {
     database = "database_in_the_source_tree.csv"
     deps = [ ":apps" ]
     optional_paths = [ "$root_build_dir/**/*.elf" ]
   }

.. note::

   The ``paths`` and ``optional_targets`` arguments do not add anything to
   ``deps``, so there is no guarantee that the referenced artifacts will exist
   when the database is updated. Provide ``targets`` or ``deps`` or build other
   GN targets first if this is a concern.

CMake integration
=================
Token databases may be updated or created as part of a CMake build. The
``pw_tokenizer_database`` template provided by
``$dir_pw_tokenizer/database.cmake`` automatically updates an in-source tokenized
strings database or creates a new database with artifacts from a CMake target.

To create a new database, set the ``CREATE`` variable to the desired database
type (``"csv"`` or ``"binary"``). The database will be created in the output
directory.

.. code-block::

   include("$dir_pw_tokenizer/database.cmake")

   pw_tokenizer_database("my_database") {
     CREATE binary
     TARGET my_target.ext
     DEPS ${deps_list}
   }

To update an existing database, provide the path to the database with
the ``database`` variable.

.. code-block::

   pw_tokenizer_database("my_database") {
     DATABASE database_in_the_source_tree.csv
     TARGET my_target.ext
     DEPS ${deps_list}
   }

.. _module-pw_tokenizer-detokenization-guides:

--------------
Detokenization
--------------
See :ref:`module-pw_tokenizer-detokenization` for a conceptual overview
of detokenization.

Detokenizing command line utilities
===================================
See :ref:`module-pw_tokenizer-cli-detokenizing`.

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

.. _module-pw_tokenizer-base64-decoding:

Decoding Base64
===============
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

Investigating undecoded Base64 messages
---------------------------------------
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
^^^^^^^
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

.. _module-pw_tokenizer-collisions-guide:

----------------
Token collisions
----------------
See :ref:`module-pw_tokenizer-collisions` for a conceptual overview of token
collisions.

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

.. _module-pw_tokenizer-domains:

--------------------
Tokenization domains
--------------------
``pw_tokenizer`` supports having multiple tokenization domains. Domains are a
string label associated with each tokenized string. This allows projects to keep
tokens from different sources separate. Potential use cases include the
following:

* Keep large sets of tokenized strings separate to avoid collisions.
* Create a separate database for a small number of strings that use truncated
  tokens, for example only 10 or 16 bits instead of the full 32 bits.

If no domain is specified, the domain is empty (``""``). For many projects, this
default domain is sufficient, so no additional configuration is required.

.. code-block:: cpp

   // Tokenizes this string to the default ("") domain.
   PW_TOKENIZE_STRING("Hello, world!");

   // Tokenizes this string to the "my_custom_domain" domain.
   PW_TOKENIZE_STRING_DOMAIN("my_custom_domain", "Hello, world!");

The database and detokenization command line tools default to reading from the
default domain. The domain may be specified for ELF files by appending
``#DOMAIN_NAME`` to the file path. Use ``#.*`` to read from all domains. For
example, the following reads strings in ``some_domain`` from ``my_image.elf``.

.. code-block:: sh

   ./database.py create --database my_db.csv path/to/my_image.elf#some_domain

See :ref:`module-pw_tokenizer-managing-token-databases` for information about
the ``database.py`` command line tool.

----------
Case study
----------
.. note:: This section discusses the implementation, results, and lessons
   learned from a real-world deployment of ``pw_tokenizer``.

The tokenizer module was developed to bring tokenized logging to an
in-development product. The product already had an established text-based
logging system. Deploying tokenization was straightforward and had substantial
benefits.

Results
=======
* Log contents shrunk by over 50%, even with Base64 encoding.

  * Significant size savings for encoded logs, even using the less-efficient
    Base64 encoding required for compatibility with the existing log system.
  * Freed valuable communication bandwidth.
  * Allowed storing many more logs in crash dumps.

* Substantial flash savings.

  * Reduced the size firmware images by up to 18%.

* Simpler logging code.

  * Removed CPU-heavy ``snprintf`` calls.
  * Removed complex code for forwarding log arguments to a low-priority task.

This section describes the tokenizer deployment process and highlights key
insights.

Firmware deployment
===================
* In the project's logging macro, calls to the underlying logging function were
  replaced with a tokenized log macro invocation.
* The log level was passed as the payload argument to facilitate runtime log
  level control.
* For this project, it was necessary to encode the log messages as text. In
  the handler function the log messages were encoded in the $-prefixed
  :ref:`module-pw_tokenizer-base64-format`, then dispatched as normal log messages.
* Asserts were tokenized a callback-based API that has been removed (a
  :ref:`custom macro <module-pw_tokenizer-custom-macro>` is a better
  alternative).

.. attention::
  Do not encode line numbers in tokenized strings. This results in a huge
  number of lines being added to the database, since every time code moves,
  new strings are tokenized. If :ref:`module-pw_log_tokenized` is used, line
  numbers are encoded in the log metadata. Line numbers may also be included by
  by adding ``"%d"`` to the format string and passing ``__LINE__``.

.. _module-pw_tokenizer-database-management:

Database management
===================
* The token database was stored as a CSV file in the project's Git repo.
* The token database was automatically updated as part of the build, and
  developers were expected to check in the database changes alongside their code
  changes.
* A presubmit check verified that all strings added by a change were added to
  the token database.
* The token database included logs and asserts for all firmware images in the
  project.
* No strings were purged from the token database.

.. tip::
   Merge conflicts may be a frequent occurrence with an in-source CSV database.
   Use the :ref:`module-pw_tokenizer-directory-database-format` instead.

Decoding tooling deployment
===========================
* The Python detokenizer in ``pw_tokenizer`` was deployed to two places:

  * Product-specific Python command line tools, using
    ``pw_tokenizer.Detokenizer``.
  * Standalone script for decoding prefixed Base64 tokens in files or
    live output (e.g. from ``adb``), using ``detokenize.py``'s command line
    interface.

* The C++ detokenizer library was deployed to two Android apps with a Java
  Native Interface (JNI) layer.

  * The binary token database was included as a raw resource in the APK.
  * In one app, the built-in token database could be overridden by copying a
    file to the phone.

.. tip::
   Make the tokenized logging tools simple to use for your project.

   * Provide simple wrapper shell scripts that fill in arguments for the
     project. For example, point ``detokenize.py`` to the project's token
     databases.
   * Use ``pw_tokenizer.AutoUpdatingDetokenizer`` to decode in
     continuously-running tools, so that users don't have to restart the tool
     when the token database updates.
   * Integrate detokenization everywhere it is needed. Integrating the tools
     takes just a few lines of code, and token databases can be embedded in APKs
     or binaries.
