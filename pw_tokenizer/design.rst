.. _module-pw_tokenizer-design:

======
Design
======
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Compress strings to shrink logs by +75%

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

--------
Encoding
--------
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

------------------------------------------------------
Token generation: fixed length hashing at compile time
------------------------------------------------------
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

.. _module-pw_tokenizer-proto:

------------------------------------
Tokenized fields in protocol buffers
------------------------------------
Text may be represented in a few different ways:

- Plain ASCII or UTF-8 text (``This is plain text``)
- Base64-encoded tokenized message (``$ibafcA==``)
- Binary-encoded tokenized message (``89 b6 9f 70``)
- Little-endian 32-bit integer token (``0x709fb689``)

``pw_tokenizer`` provides tools for working with protobuf fields that may
contain tokenized text.

See :ref:`module-pw_tokenizer-protobuf-tokenization-python` for guidance
on tokenizing protobufs in Python.

Tokenized field protobuf option
===============================
``pw_tokenizer`` provides the ``pw.tokenizer.format`` protobuf field option.
This option may be applied to a protobuf field to indicate that it may contain a
tokenized string. A string that is optionally tokenized is represented with a
single ``bytes`` field annotated with ``(pw.tokenizer.format) =
TOKENIZATION_OPTIONAL``.

For example, the following protobuf has one field that may contain a tokenized
string.

.. code-block:: protobuf

  message MessageWithOptionallyTokenizedField {
    bytes just_bytes = 1;
    bytes maybe_tokenized = 2 [(pw.tokenizer.format) = TOKENIZATION_OPTIONAL];
    string just_text = 3;
  }

Decoding optionally tokenized strings
=====================================
The encoding used for an optionally tokenized field is not recorded in the
protobuf. Despite this, the text can reliably be decoded. This is accomplished
by attempting to decode the field as binary or Base64 tokenized data before
treating it like plain text.

The following diagram describes the decoding process for optionally tokenized
fields in detail.

.. mermaid::

  flowchart TD
     start([Received bytes]) --> binary

     binary[Decode as<br>binary tokenized] --> binary_ok
     binary_ok{Detokenizes<br>successfully?} -->|no| utf8
     binary_ok -->|yes| done_binary([Display decoded binary])

     utf8[Decode as UTF-8] --> utf8_ok
     utf8_ok{Valid UTF-8?} -->|no| base64_encode
     utf8_ok -->|yes| base64

     base64_encode[Encode as<br>tokenized Base64] --> display
     display([Display encoded Base64])

     base64[Decode as<br>Base64 tokenized] --> base64_ok

     base64_ok{Fully<br>or partially<br>detokenized?} -->|no| is_plain_text
     base64_ok -->|yes| base64_results

     is_plain_text{Text is<br>printable?} -->|no| base64_encode
     is_plain_text-->|yes| plain_text

     base64_results([Display decoded Base64])
     plain_text([Display text])

Potential decoding problems
---------------------------
The decoding process for optionally tokenized fields will yield correct results
in almost every situation. In rare circumstances, it is possible for it to fail,
but these can be avoided with a low-overhead mitigation if desired.

There are two ways in which the decoding process may fail.

Accidentally interpreting plain text as tokenized binary
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
If a plain-text string happens to decode as a binary tokenized message, the
incorrect message could be displayed. This is very unlikely to occur. While many
tokens will incidentally end up being valid UTF-8 strings, it is highly unlikely
that a device will happen to log one of these strings as plain text. The
overwhelming majority of these strings will be nonsense.

If an implementation wishes to guard against this extremely improbable
situation, it is possible to prevent it. This situation is prevented by
appending 0xFF (or another byte never valid in UTF-8) to binary tokenized data
that happens to be valid UTF-8 (or all binary tokenized messages, if desired).
When decoding, if there is an extra 0xFF byte, it is discarded.

Displaying undecoded binary as plain text instead of Base64
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
If a message fails to decode as binary tokenized and it is not valid UTF-8, it
is displayed as tokenized Base64. This makes it easily recognizable as a
tokenized message and makes it simple to decode later from the text output (for
example, with an updated token database).

A binary message for which the token is not known may coincidentally be valid
UTF-8 or ASCII. 6.25% of 4-byte sequences are composed only of ASCII characters.
When decoding with an out-of-date token database, it is possible that some
binary tokenized messages will be displayed as plain text rather than tokenized
Base64.

This situation is likely to occur, but should be infrequent. Even if it does
happen, it is not a serious issue. A very small number of strings will be
displayed incorrectly, but these strings cannot be decoded anyway. One nonsense
string (e.g. ``a-D1``) would be displayed instead of another (``$YS1EMQ==``).
Updating the token database would resolve the issue, though the non-Base64 logs
would be difficult decode later from a log file.

This situation can be avoided with the same approach described in
`Accidentally interpreting plain text as tokenized binary`_. Appending
an invalid UTF-8 character prevents the undecoded binary message from being
interpreted as plain text.

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

See :ref:`module-pw_tokenizer-base64-guides` for guidance on encoding and
decoding Base64 messages.

.. _module-pw_tokenizer-token-databases:

---------------
Token databases
---------------
Token databases store a mapping of tokens to the strings they represent. An ELF
file can be used as a token database, but it only contains the strings for its
exact build. A token database file aggregates tokens from multiple ELF files, so
that a single database can decode tokenized strings from any known ELF.

Token databases contain the token, removal date (if any), and string for each
tokenized string.

For help with using token databases, see
:ref:`module-pw_tokenizer-managing-token-databases`.

Token database formats
======================
Three token database formats are supported: CSV, binary, and directory. Tokens
may also be read from ELF files or ``.a`` archives, but cannot be written to
these formats.

CSV database format
-------------------
The CSV database format has three columns: the token in hexadecimal, the removal
date (if any) in year-month-day format, and the string literal, surrounded by
quotes. Quote characters within the string are represented as two quote
characters.

This example database contains six strings, three of which have removal dates.

.. code-block::

   141c35d5,          ,"The answer: ""%s"""
   2e668cd6,2019-12-25,"Jello, world!"
   7b940e2a,          ,"Hello %s! %hd %e"
   851beeb6,          ,"%u %d"
   881436a0,2020-01-01,"The answer is: %s"
   e13b0f94,2020-04-01,"%llu"

Binary database format
----------------------
The binary database format is comprised of a 16-byte header followed by a series
of 8-byte entries. Each entry stores the token and the removal date, which is
0xFFFFFFFF if there is none. The string literals are stored next in the same
order as the entries. Strings are stored with null terminators. See
`token_database.h <https://pigweed.googlesource.com/pigweed/pigweed/+/HEAD/pw_tokenizer/public/pw_tokenizer/token_database.h>`_
for full details.

The binary form of the CSV database is shown below. It contains the same
information, but in a more compact and easily processed form. It takes 141 B
compared with the CSV database's 211 B.

.. code-block:: text

   [header]
   0x00: 454b4f54 0000534e  TOKENS..
   0x08: 00000006 00000000  ........

   [entries]
   0x10: 141c35d5 ffffffff  .5......
   0x18: 2e668cd6 07e30c19  ..f.....
   0x20: 7b940e2a ffffffff  *..{....
   0x28: 851beeb6 ffffffff  ........
   0x30: 881436a0 07e40101  .6......
   0x38: e13b0f94 07e40401  ..;.....

   [string table]
   0x40: 54 68 65 20 61 6e 73 77 65 72 3a 20 22 25 73 22  The answer: "%s"
   0x50: 00 4a 65 6c 6c 6f 2c 20 77 6f 72 6c 64 21 00 48  .Jello, world!.H
   0x60: 65 6c 6c 6f 20 25 73 21 20 25 68 64 20 25 65 00  ello %s! %hd %e.
   0x70: 25 75 20 25 64 00 54 68 65 20 61 6e 73 77 65 72  %u %d.The answer
   0x80: 20 69 73 3a 20 25 73 00 25 6c 6c 75 00            is: %s.%llu.

.. _module-pw_tokenizer-directory-database-format:

Directory database format
-------------------------
pw_tokenizer can consume directories of CSV databases. A directory database
will be searched recursively for files with a `.pw_tokenizer.csv` suffix, all
of which will be used for subsequent detokenization lookups.

An example directory database might look something like this:

.. code-block:: text

   token_database
   ├── chuck_e_cheese.pw_tokenizer.csv
   ├── fungi_ble.pw_tokenizer.csv
   └── some_more
       └── arcade.pw_tokenizer.csv

This format is optimized for storage in a Git repository alongside source code.
The token database commands randomly generate unique file names for the CSVs in
the database to prevent merge conflicts. Running ``mark_removed`` or ``purge``
commands in the database CLI consolidates the files to a single CSV.

The database command line tool supports a ``--discard-temporary
<upstream_commit>`` option for ``add``. In this mode, the tool attempts to
discard temporary tokens. It identifies the latest CSV not present in the
provided ``<upstream_commit>``, and tokens present that CSV that are not in the
newly added tokens are discarded. This helps keep temporary tokens (e.g from
debug logs) out of the database.

JSON support
============
While pw_tokenizer doesn't specify a JSON database format, a token database can
be created from a JSON formatted array of strings. This is useful for side-band
token database generation for strings that are not embedded as parsable tokens
in compiled binaries. See :ref:`module-pw_tokenizer-database-creation` for
instructions on generating a token database from a JSON file.

.. _module-pw_tokenizer-collisions:

----------------
Token collisions
----------------
Tokens are calculated with a hash function. It is possible for different
strings to hash to the same token. When this happens, multiple strings will have
the same token in the database, and it may not be possible to unambiguously
decode a token.

The detokenization tools attempt to resolve collisions automatically. Collisions
are resolved based on two things:

- whether the tokenized data matches the strings arguments' (if any), and
- if / when the string was marked as having been removed from the database.

See :ref:`module-pw_tokenizer-collisions-guide` for guidance on how to fix
collisions.

Probability of collisions
=========================
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

Keep this table in mind when masking tokens (see
:ref:`module-pw_tokenizer-masks`). 16 bits might be acceptable when
tokenizing a small set of strings, such as module names, but won't be suitable
for large sets of strings, like log messages.

.. _module-pw_tokenizer-detokenization:

--------------
Detokenization
--------------
Detokenization is the process of expanding a token to the string it represents
and decoding its arguments. ``pw_tokenizer`` provides Python, C++ and
TypeScript detokenization libraries.

**Example: decoding tokenized logs**

A project might tokenize its log messages with the
:ref:`module-pw_tokenizer-base64-format`. Consider the following log file, which
has four tokenized logs and one plain text log:

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

   This example uses the :ref:`module-pw_tokenizer-base64-format`, which
   occupies about 4/3 (133%) as much space as the default binary format when
   encoded. For projects that wish to interleave tokenized with plain text,
   using Base64 is a worthwhile tradeoff.

See :ref:`module-pw_tokenizer-detokenization-guides` for detailed instructions
on how to do detokenization in different programming languages.

.. _module-pw_tokenizer-python-detokenization-c99-printf-notes:

Python detokenization: C99 ``printf`` compatibility notes
=========================================================
This implementation is designed to align with the
`C99 specification, section 7.19.6
<https://www.dii.uchile.cl/~daespino/files/Iso_C_1999_definition.pdf>`_.
Notably, this specification is slightly different than what is implemented
in most compilers due to each compiler choosing to interpret undefined
behavior in slightly different ways. Treat the following description as the
source of truth.

This implementation supports:

- Overall Format: ``%[flags][width][.precision][length][specifier]``
- Flags (Zero or More)
   - ``-``: Left-justify within the given field width; Right justification is
     the default (see Width modifier).
   - ``+``: Forces to preceed the result with a plus or minus sign (``+`` or
     ``-``) even for positive numbers. By default, only negative numbers are
     preceded with a ``-`` sign.
   - (space): If no sign is going to be written, a blank space is inserted
     before the value.
   - ``#``: Specifies an alternative print syntax should be used.
      - Used with ``o``, ``x`` or ``X`` specifiers the value is preceeded with
        ``0``, ``0x`` or ``0X``, respectively, for values different than zero.
      - Used with ``a``, ``A``, ``e``, ``E``, ``f``, ``F``, ``g``, or ``G`` it
        forces the written output to contain a decimal point even if no more
        digits follow. By default, if no digits follow, no decimal point is
        written.
   - ``0``: Left-pads the number with zeroes (``0``) instead of spaces when
     padding is specified (see width sub-specifier).
- Width (Optional)
   - ``(number)``: Minimum number of characters to be printed. If the value to
     be printed is shorter than this number, the result is padded with blank
     spaces or ``0`` if the ``0`` flag is present. The value is not truncated
     even if the result is larger. If the value is negative and the ``0`` flag
     is present, the ``0``\s are padded after the ``-`` symbol.
   - ``*``: The width is not specified in the format string, but as an
     additional integer value argument preceding the argument that has to be
     formatted.
- Precision (Optional)
   - ``.(number)``
      - For ``d``, ``i``, ``o``, ``u``, ``x``, ``X``, specifies the minimum
        number of digits to be written. If the value to be written is shorter
        than this number, the result is padded with leading zeros. The value is
        not truncated even if the result is longer.

        - A precision of ``0`` means that no character is written for the value
          ``0``.

      - For ``a``, ``A``, ``e``, ``E``, ``f``, and ``F``, specifies the number
        of digits to be printed after the decimal point. By default, this is
        ``6``.

      - For ``g`` and ``G``, specifies the maximum number of significant digits
        to be printed.

      - For ``s``, specifies the maximum number of characters to be printed. By
        default all characters are printed until the ending null character is
        encountered.

      - If the period is specified without an explicit value for precision,
        ``0`` is assumed.
   - ``.*``: The precision is not specified in the format string, but as an
     additional integer value argument preceding the argument that has to be
     formatted.
- Length (Optional)
   - ``hh``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``signed char`` or ``unsigned char``.
     However, this is largely ignored in the implementation due to it not being
     necessary for Python or argument decoding (since the argument is always
     encoded at least as a 32-bit integer).
   - ``h``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``signed short int`` or
     ``unsigned short int``. However, this is largely ignored in the
     implementation due to it not being necessary for Python or argument
     decoding (since the argument is always encoded at least as a 32-bit
     integer).
   - ``l``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``signed long int`` or
     ``unsigned long int``. Also is usable with ``c`` and ``s`` to specify that
     the arguments will be encoded with ``wchar_t`` values (which isn't
     different from normal ``char`` values). However, this is largely ignored in
     the implementation due to it not being necessary for Python or argument
     decoding (since the argument is always encoded at least as a 32-bit
     integer).
   - ``ll``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``signed long long int`` or
     ``unsigned long long int``. This is required to properly decode the
     argument as a 64-bit integer.
   - ``L``: Usable with ``a``, ``A``, ``e``, ``E``, ``f``, ``F``, ``g``, or
     ``G`` conversion specifiers applies to a long double argument. However,
     this is ignored in the implementation due to floating point value encoded
     that is unaffected by bit width.
   - ``j``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``intmax_t`` or ``uintmax_t``.
   - ``z``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``size_t``. This will force the argument
     to be decoded as an unsigned integer.
   - ``t``: Usable with ``d``, ``i``, ``o``, ``u``, ``x``, or ``X`` specifiers
     to convey the argument will be a ``ptrdiff_t``.
   - If a length modifier is provided for an incorrect specifier, it is ignored.
- Specifier (Required)
   - ``d`` / ``i``: Used for signed decimal integers.

   - ``u``: Used for unsigned decimal integers.

   - ``o``: Used for unsigned decimal integers and specifies formatting should
     be as an octal number.

   - ``x``: Used for unsigned decimal integers and specifies formatting should
     be as a hexadecimal number using all lowercase letters.

   - ``X``: Used for unsigned decimal integers and specifies formatting should
     be as a hexadecimal number using all uppercase letters.

   - ``f``: Used for floating-point values and specifies to use lowercase,
     decimal floating point formatting.

     - Default precision is ``6`` decimal places unless explicitly specified.

   - ``F``: Used for floating-point values and specifies to use uppercase,
     decimal floating point formatting.

     - Default precision is ``6`` decimal places unless explicitly specified.

   - ``e``: Used for floating-point values and specifies to use lowercase,
     exponential (scientific) formatting.

     - Default precision is ``6`` decimal places unless explicitly specified.

   - ``E``: Used for floating-point values and specifies to use uppercase,
     exponential (scientific) formatting.

     - Default precision is ``6`` decimal places unless explicitly specified.

   - ``g``: Used for floating-point values and specified to use ``f`` or ``e``
     formatting depending on which would be the shortest representation.

     - Precision specifies the number of significant digits, not just digits
       after the decimal place.

     - If the precision is specified as ``0``, it is interpreted to mean ``1``.

     - ``e`` formatting is used if the the exponent would be less than ``-4`` or
       is greater than or equal to the precision.

     - Trailing zeros are removed unless the ``#`` flag is set.

     - A decimal point only appears if it is followed by a digit.

     - ``NaN`` or infinities always follow ``f`` formatting.

   - ``G``: Used for floating-point values and specified to use ``f`` or ``e``
     formatting depending on which would be the shortest representation.

     - Precision specifies the number of significant digits, not just digits
       after the decimal place.

     - If the precision is specified as ``0``, it is interpreted to mean ``1``.

     - ``E`` formatting is used if the the exponent would be less than ``-4`` or
       is greater than or equal to the precision.

     - Trailing zeros are removed unless the ``#`` flag is set.

     - A decimal point only appears if it is followed by a digit.

     - ``NaN`` or infinities always follow ``F`` formatting.

   - ``c``: Used for formatting a ``char`` value.

   - ``s``: Used for formatting a string of ``char`` values.

     - If width is specified, the null terminator character is included as a
       character for width count.

     - If precision is specified, no more ``char``\s than that value will be
       written from the string (padding is used to fill additional width).

   - ``p``: Used for formatting a pointer address.

   - ``%``: Prints a single ``%``. Only valid as ``%%`` (supports no flags,
     width, precision, or length modifiers).

Underspecified details:

- If both ``+`` and (space) flags appear, the (space) is ignored.
- The ``+`` and (space) flags will error if used with ``c`` or ``s``.
- The ``#`` flag will error if used with ``d``, ``i``, ``u``, ``c``, ``s``, or
  ``p``.
- The ``0`` flag will error if used with ``c``, ``s``, or ``p``.
- Both ``+`` and (space) can work with the unsigned integer specifiers ``u``,
  ``o``, ``x``, and ``X``.
- If a length modifier is provided for an incorrect specifier, it is ignored.
- The ``z`` length modifier will decode arugments as signed as long as ``d`` or
  ``i`` is used.
- ``p`` is implementation defined.

  - For this implementation, it will print with a ``0x`` prefix and then the
    pointer value was printed using ``%08X``.

  - ``p`` supports the ``+``, ``-``, and (space) flags, but not the ``#`` or
    ``0`` flags.

  - None of the length modifiers are usable with ``p``.

  - This implementation will try to adhere to user-specified width (assuming the
    width provided is larger than the guaranteed minimum of ``10``).

  - Specifying precision for ``p`` is considered an error.
- Only ``%%`` is allowed with no other modifiers. Things like ``%+%`` will fail
  to decode. Some C stdlib implementations support any modifiers being
  present between ``%``, but ignore any for the output.
- If a width is specified with the ``0`` flag for a negative value, the padded
  ``0``\s will appear after the ``-`` symbol.
- A precision of ``0`` for ``d``, ``i``, ``u``, ``o``, ``x``, or ``X`` means
  that no character is written for the value ``0``.
- Precision cannot be specified for ``c``.
- Using ``*`` or fixed precision with the ``s`` specifier still requires the
  string argument to be null-terminated. This is due to argument encoding
  happening on the C/C++-side while the precision value is not read or
  otherwise used until decoding happens in this Python code.

Non-conformant details:

- ``n`` specifier: We do not support the ``n`` specifier since it is impossible
  for us to retroactively tell the original program how many characters have
  been printed since this decoding happens a great deal of time after the
  device sent it, usually on a separate processing device entirely.

---------------------------
Limitations and future work
---------------------------

GCC bug: tokenization in template functions
===========================================
GCC incorrectly ignores the section attribute for template `functions
<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70435>`_ and `variables
<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88061>`_. For example, the
following won't work when compiling with GCC and tokenized logging:

.. code-block:: cpp

   template <...>
   void DoThings() {
     int value = GetValue();
     // This log won't work with tokenized logs due to the templated context.
     PW_LOG_INFO("Got value: %d", value);
     ...
   }

The bug causes tokenized strings in template functions to be emitted into
``.rodata`` instead of the special tokenized string section. This causes two
problems:

1. Tokenized strings will not be discovered by the token database tools.
2. Tokenized strings may not be removed from the final binary.

There are two workarounds.

#. **Use Clang.** Clang puts the string data in the requested section, as
   expected. No extra steps are required.

#. **Move tokenization calls to a non-templated context.** Creating a separate
   non-templated function and invoking it from the template resolves the issue.
   This enables tokenizing in most cases encountered in practice with
   templates.

   .. code-block:: cpp

      // In .h file:
      void LogThings(value);

      template <...>
      void DoThings() {
        int value = GetValue();
        // This log will work: calls non-templated helper.
        LogThings(value);
        ...
      }

      // In .cc file:
      void LogThings(int value) {
        // Tokenized logging works as expected in this non-templated context.
        PW_LOG_INFO("Got value %d", value);
      }

There is a third option, which isn't implemented yet, which is to compile the
binary twice: once to extract the tokens, and once for the production binary
(without tokens). If this is interesting to you please get in touch.

64-bit tokenization
===================
The Python and C++ detokenizing libraries currently assume that strings were
tokenized on a system with 32-bit ``long``, ``size_t``, ``intptr_t``, and
``ptrdiff_t``. Decoding may not work correctly for these types if a 64-bit
device performed the tokenization.

Supporting detokenization of strings tokenized on 64-bit targets would be
simple. This could be done by adding an option to switch the 32-bit types to
64-bit. The tokenizer stores the sizes of these types in the
``.pw_tokenizer.info`` ELF section, so the sizes of these types can be verified
by checking the ELF file, if necessary.

Tokenization in headers
=======================
Tokenizing code in header files (inline functions or templates) may trigger
warnings such as ``-Wlto-type-mismatch`` under certain conditions. That
is because tokenization requires declaring a character array for each tokenized
string. If the tokenized string includes macros that change value, the size of
this character array changes, which means the same static variable is defined
with different sizes. It should be safe to suppress these warnings, but, when
possible, code that tokenizes strings with macros that can change value should
be moved to source files rather than headers.

.. _module-pw_tokenizer-tokenized-strings-as-args:

Tokenized strings as ``%s`` arguments
=====================================
Encoding ``%s`` string arguments is inefficient, since ``%s`` strings are
encoded 1:1, with no tokenization. It would be better to send a tokenized string
literal as an integer instead of a string argument, but this is not yet
supported.

A string token could be sent by marking an integer % argument in a way
recognized by the detokenization tools. The detokenizer would expand the
argument to the string represented by the integer.

.. code-block:: cpp

   #define PW_TOKEN_ARG PRIx32 "<PW_TOKEN]"

   constexpr uint32_t answer_token = PW_TOKENIZE_STRING("Uh, who is there");

   PW_TOKENIZE_STRING("Knock knock: %" PW_TOKEN_ARG "?", answer_token);

Strings with arguments could be encoded to a buffer, but since printf strings
are null-terminated, a binary encoding would not work. These strings can be
prefixed Base64-encoded and sent as ``%s`` instead. See
:ref:`module-pw_tokenizer-base64-format`.

Another possibility: encode strings with arguments to a ``uint64_t`` and send
them as an integer. This would be efficient and simple, but only support a small
number of arguments.

--------------------
Deployment war story
--------------------
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
