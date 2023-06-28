.. _module-pw_tokenizer-design:

======
Design
======
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Cut your log sizes in half
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli

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

.. _module-pw_tokenizer-design-example:

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

-------------
Compatibility
-------------
* C11
* C++14
* Python 3

------------
Dependencies
------------
* ``pw_varint`` module
* ``pw_preprocessor`` module
* ``pw_span`` module

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

----------------------------------
C99 ``printf`` Compatibility Notes
----------------------------------
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
