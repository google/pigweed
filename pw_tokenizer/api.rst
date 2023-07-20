.. _module-pw_tokenizer-api:

=============
API reference
=============
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Cut your log sizes in half
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli

.. _module-pw_tokenizer-api-tokenization:

------------
Tokenization
------------
Tokenization converts a string literal to a token. If it's a printf-style
string, its arguments are encoded along with it. The results of tokenization can
be sent off device or stored in place of a full string.

.. doxygentypedef:: pw_tokenizer_Token

Tokenization macros
===================
Adding tokenization to a project is simple. To tokenize a string, include
``pw_tokenizer/tokenize.h`` and invoke one of the ``PW_TOKENIZE_`` macros.

Tokenize a string literal
-------------------------
``pw_tokenizer`` provides macros for tokenizing string literals with no
arguments.

.. doxygendefine:: PW_TOKENIZE_STRING
.. doxygendefine:: PW_TOKENIZE_STRING_DOMAIN
.. doxygendefine:: PW_TOKENIZE_STRING_MASK

The tokenization macros above cannot be used inside other expressions.

.. admonition:: **Yes**: Assign :c:macro:`PW_TOKENIZE_STRING` to a ``constexpr`` variable.
  :class: checkmark

  .. code:: cpp

    constexpr uint32_t kGlobalToken = PW_TOKENIZE_STRING("Wowee Zowee!");

    void Function() {
      constexpr uint32_t local_token = PW_TOKENIZE_STRING("Wowee Zowee?");
    }

.. admonition:: **No**: Use :c:macro:`PW_TOKENIZE_STRING` in another expression.
  :class: error

  .. code:: cpp

   void BadExample() {
     ProcessToken(PW_TOKENIZE_STRING("This won't compile!"));
   }

  Use :c:macro:`PW_TOKENIZE_STRING_EXPR` instead.

An alternate set of macros are provided for use inside expressions. These make
use of lambda functions, so while they can be used inside expressions, they
require C++ and cannot be assigned to constexpr variables or be used with
special function variables like ``__func__``.

.. doxygendefine:: PW_TOKENIZE_STRING_EXPR
.. doxygendefine:: PW_TOKENIZE_STRING_DOMAIN_EXPR
.. doxygendefine:: PW_TOKENIZE_STRING_MASK_EXPR

.. admonition:: When to use these macros

  Use :c:macro:`PW_TOKENIZE_STRING` and related macros to tokenize string
  literals that do not need %-style arguments encoded.

.. admonition:: **Yes**: Use :c:macro:`PW_TOKENIZE_STRING_EXPR` within other expressions.
  :class: checkmark

  .. code:: cpp

    void GoodExample() {
      ProcessToken(PW_TOKENIZE_STRING_EXPR("This will compile!"));
    }

.. admonition:: **No**: Assign :c:macro:`PW_TOKENIZE_STRING_EXPR` to a ``constexpr`` variable.
  :class: error

  .. code:: cpp

     constexpr uint32_t wont_work = PW_TOKENIZE_STRING_EXPR("This won't compile!"));

  Instead, use :c:macro:`PW_TOKENIZE_STRING` to assign to a ``constexpr`` variable.

.. admonition:: **No**: Tokenize ``__func__`` in :c:macro:`PW_TOKENIZE_STRING_EXPR`.
  :class: error

  .. code:: cpp

    void BadExample() {
      // This compiles, but __func__ will not be the outer function's name, and
      // there may be compiler warnings.
      constexpr uint32_t wont_work = PW_TOKENIZE_STRING_EXPR(__func__);
    }

  Instead, use :c:macro:`PW_TOKENIZE_STRING` to tokenize ``__func__`` or similar macros.

Tokenize a message with arguments to a buffer
---------------------------------------------
.. doxygendefine:: PW_TOKENIZE_TO_BUFFER
.. doxygendefine:: PW_TOKENIZE_TO_BUFFER_DOMAIN
.. doxygendefine:: PW_TOKENIZE_TO_BUFFER_MASK

.. admonition:: Why use this macro

   - Encode a tokenized message for consumption within a function.
   - Encode a tokenized message into an existing buffer.

   Avoid using ``PW_TOKENIZE_TO_BUFFER`` in widely expanded macros, such as a
   logging macro, because it will result in larger code size than passing the
   tokenized data to a function.

.. _module-pw_tokenizer-custom-macro:

Tokenize a message with arguments in a custom macro
---------------------------------------------------
Projects can leverage the tokenization machinery in whichever way best suits
their needs. The most efficient way to use ``pw_tokenizer`` is to pass tokenized
data to a global handler function. A project's custom tokenization macro can
handle tokenized data in a function of their choosing.

``pw_tokenizer`` provides two low-level macros for projects to use
to create custom tokenization macros.

.. doxygendefine:: PW_TOKENIZE_FORMAT_STRING
.. doxygendefine:: PW_TOKENIZER_ARG_TYPES

The outputs of these macros are typically passed to an encoding function. That
function encodes the token, argument types, and argument data to a buffer using
helpers provided by ``pw_tokenizer/encode_args.h``.

.. doxygenfunction:: pw::tokenizer::EncodeArgs
.. doxygenclass:: pw::tokenizer::EncodedMessage
   :members:
.. doxygenfunction:: pw_tokenizer_EncodeArgs

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

Encoding
========
The token is a 32-bit hash calculated during compilation. The string is encoded
little-endian with the token followed by arguments, if any. For example, the
31-byte string ``You can go about your business.`` hashes to 0xdac9a244.
This is encoded as 4 bytes: ``44 a2 c9 da``.

Arguments are encoded as follows:

* **Integers**  (1--10 bytes) --
  `ZagZag and varint encoded <https://developers.google.com/protocol-buffers/docs/encoding#signed-integers>`_,
  similarly to Protocol Buffers. Smaller values take fewer bytes.
* **Floating point numbers** (4 bytes) -- Single precision floating point.
* **Strings** (1--128 bytes) -- Length byte followed by the string contents.
  The top bit of the length whether the string was truncated or not. The
  remaining 7 bits encode the string length, with a maximum of 127 bytes.

.. TODO(hepler): insert diagram here!

.. tip::
   ``%s`` arguments can quickly fill a tokenization buffer. Keep ``%s``
   arguments short or avoid encoding them as strings (e.g. encode an enum as an
   integer instead of a string). See also
   :ref:`module-pw_tokenizer-tokenized-strings-as-args`.

Buffer sizing helper
--------------------
.. doxygenfunction:: pw::tokenizer::MinEncodingBufferSizeBytes

Token generation: fixed length hashing at compile time
======================================================
String tokens are generated using a modified version of the x65599 hash used by
the SDBM project. All hashing is done at compile time.

In C code, strings are hashed with a preprocessor macro. For compatibility with
macros, the hash must be limited to a fixed maximum number of characters. This
value is set by ``PW_TOKENIZER_CFG_C_HASH_LENGTH``. Increasing
``PW_TOKENIZER_CFG_C_HASH_LENGTH`` increases the compilation time for C due to
the complexity of the hashing macros.

C++ macros use a constexpr function instead of a macro. This function works with
any length of string and has lower compilation time impact than the C macros.
For consistency, C++ tokenization uses the same hash algorithm, but the
calculated values will differ between C and C++ for strings longer than
``PW_TOKENIZER_CFG_C_HASH_LENGTH`` characters.

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

Protobuf tokenization library
-----------------------------
The ``pw_tokenizer.proto`` Python module defines functions that may be used to
detokenize protobuf objects in Python. The function
:func:`pw_tokenizer.proto.detokenize_fields` detokenizes all fields annotated as
tokenized, replacing them with their detokenized version. For example:

.. code-block:: python

  my_detokenizer = pw_tokenizer.Detokenizer(some_database)

  my_message = SomeMessage(tokenized_field=b'$YS1EMQ==')
  pw_tokenizer.proto.detokenize_fields(my_detokenizer, my_message)

  assert my_message.tokenized_field == b'The detokenized string! Cool!'

pw_tokenizer.proto
^^^^^^^^^^^^^^^^^^
.. automodule:: pw_tokenizer.proto
  :members:
