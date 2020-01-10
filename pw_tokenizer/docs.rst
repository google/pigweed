.. _chapter-tokenizer:

.. default-domain:: cpp

.. highlight:: sh

------------
pw_tokenizer
------------
The tokenizer module provides facilities for converting strings to binary
tokens. String literals are replaced with integer tokens in the firmware image,
which can be decoded off-device to restore the original string. Strings may be
printf-style format strings and include arguments, such as ``"My name is %s"``.
Arguments are encoded into compact binary form at runtime.

.. note::
  This usage of the term "tokenizer" is not related to parsing! The
  module is called tokenizer because it replaces a whole string literal with an
  integer token. It does not parse strings into separate tokens.

The most common application of the tokenizer module is binary logging, and its
designed to integrate easily into existing logging systems. However, the
tokenizer is general purpose and can be used to tokenize any strings.

**Why tokenize strings?**

  * Dramatically reduce binary size by removing string literals from binaries.
  * Reduce CPU usage by replacing snprintf calls with simple tokenization code.
  * Reduce I/O traffic, RAM, and flash usage by sending and storing compact
    tokens instead of strings.
  * Remove potentially sensitive log, assert, and other strings from binaries.

Basic operation
===============
  1. In C or C++ code, strings are hashed to generate a stable 32-bit token.
  2. The tokenization macro removes the string literal by placing it in an ELF
     section that is excluded from the final binary.
  3. Strings are extracted from an ELF to build a database of tokenized strings
     for use by the detokenizer. The ELF file may also be used directly.
  4. During operation, the device encodes the string token and its arguments, if
     any.
  5. The encoded tokenized strings are sent off-device or stored.
  6. Off-device, the detokenizer tools use the token database or ELF files to
     detokenize the strings to human-readable form.

Module usage
============
There are two sides to tokenization: tokenizing strings in the source code and
detokenizing these strings back to human-readable form.

Tokenization
------------
Tokenization converts a string literal to a token. If it's a printf-style
string, its arguments are encoded along with it. The results of tokenization can
be sent off device or stored in place of a full string.

Adding tokenization to a project is simple. To tokenize a string, include
``pw_tokenizer/tokenize.h`` and invoke a ``PW_TOKENIZE_`` macro.

To tokenize a string literal, invoke ``PW_TOKENIZE_STRING``. This macro returns
a ``uint32_t`` token.

.. code-block:: cpp

  constexpr uint32_t token = PW_TOKENIZE_STRING("Any string literal!");

Format strings are tokenized into a fixed-size buffer. The buffer contains the
``uint32_t`` token followed by the encoded form of the arguments, if any. The
most flexible tokenization macro is ``PW_TOKENIZE_TO_BUFFER``, which encodes to
a caller-provided buffer.

.. code-block:: cpp

  uint8_t buffer[BUFFER_SIZE];
  size_t size_bytes = sizeof(buffer);
  PW_TOKENIZE_TO_BUFFER(buffer, &size_bytes, format_string_literal, args...);

While ``PW_TOKENIZE_TO_BUFFER`` is flexible, its per-use code size overhead is
larger than its alternatives. ``PW_TOKENIZE_TO_CALLBACK`` tokenizes to a buffer
on the stack and calls a ``void(const uint8_t* buffer, size_t buffer_size)``
callback that is provided at the call site. The size of the buffer is set with
``PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES``.

.. code-block:: cpp

  PW_TOKENIZE_TO_CALLBACK(HandlerFunction, "Format string: %x", arg);

``PW_TOKENIZE_TO_GLOBAL_HANDLER`` is the most efficient tokenization function,
since it takes the fewest arguments. Like the callback form, it encodes to a
buffer on the stack. It then calls the C-linkage function
``pw_TokenizerHandleEncodedMessage``, which must be defined by the project.

.. code-block:: cpp

  PW_TOKENIZE_TO_GLOBAL_HANDLER(format_string_literal, arguments...);

  void pw_TokenizerHandleEncodedMessage(const uint8_t encoded_message[],
                                        size_t size_bytes);

``PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD`` is similar, but passes a
``void*`` argument to the global handler function. This can be used to pass a
log level or other metadata along with the tokenized string.

.. code-block:: cpp

  PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD(payload,
                                             format_string_literal,
                                             args...);

  void pw_TokenizerHandleEncodedMessageWithPayload(void* payload,
                                                   const uint8_t encoded_message[],
                                                   size_t size_bytes);

.. tip::
  ``%s`` arguments are inefficient to encode and can quickly fill a tokenization
  buffer. Keep ``%s`` arguments short or avoid encoding them as strings if
  possible. See `Tokenized strings as %s arguments`_.

Example: binary logging
^^^^^^^^^^^^^^^^^^^^^^^
String tokenization is perfect for logging. Consider the following log macro,
which gathers the file, line number, and log message. It calls the ``RecordLog``
function, which formats the log string, collects a timestamp, and transmits the
result.

.. code-block:: cpp

  #define LOG_INFO(format, ...) \
      RecordLog(LogLevel_INFO, __FILE_NAME__, __LINE__, format, ##__VA_ARGS__)

  void RecordLog(LogLevel level, const char* file, int line, const char* format,
                 ...) {
    if (level < current_log_level) {
      return;
    }

    int bytes = snprintf(buffer, sizeof(buffer), "%s:%d ", file, line);

    va_list args;
    va_start(args, format);
    bytes += vsnprintf(&buffer[bytes], sizeof(buffer) - bytes, format, args);
    va_end(args);

    TransmitLog(TimeSinceBootMillis(), buffer, size);
  }

It is trivial to convert this to a binary log using the tokenizer. The
``RecordLog`` call is replaced with a
``PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD`` invocation. The
``pw_TokenizerHandleEncodedMessageWithPayload`` implementation collects the
timestamp and transmits the message with ``TransmitLog``.

.. code-block:: cpp

  #define LOG_INFO(format, ...)                   \
      PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD( \
          (void*)LogLevel_INFO,                   \
          __FILE_NAME__ ":%d " format,            \
          __LINE__,                               \
          __VA_ARGS__);                           \

  extern "C" void pw_TokenizerHandleEncodedMessageWithPayload(
      void* level, const uint8_t encoded_message[], size_t size_bytes) {
    if (reinterpret_cast<LogLevel>(level) >= current_log_level) {
      TransmitLog(TimeSinceBootMillis(), encoded_message, size_bytes);
    }
  }

Note that the ``__FILE_NAME__`` string is directly included in the log format
string. Since the string is tokenized, this has no effect on binary size. A
``%d`` for the line number is added to the format string, so that changing the
line of the log message does not generate a new token. There is no overhead for
additional tokens, but it may not be desireable to fill a token database with
duplicate log lines.

Database management
^^^^^^^^^^^^^^^^^^^
Token databases store a mapping of tokens to the strings they represent. An ELF
file can be used as a token database, but it only contains the strings for its
exact build. A token database file aggregates tokens from multiple ELF files, so
that a single database can decode tokenized strings from any known ELF.

Creating and maintaining a token database is simple. Token databases are managed
with the ``database.py`` script. The ``create`` command makes a new token
database from ELF files or other databases.

.. code-block:: sh

  ./database.py create --database DATABASE_NAME ELF_OR_DATABASE_FILE...

Two database formats are supported: CSV and binary. Provide ``--type binary`` to
``create`` generate a binary database instead of the default CSV. CSV databases
are great for checking into a source control or for human review. Binary
databases are more compact and simpler to parse. The C++ detokenizer library
only supports binary databases currently.

As new tokenized strings are added, update the database with the add command.

.. code-block:: sh

  ./database.py add --database DATABASE_NAME ELF_OR_DATABASE_FILE...

A CSV token database can be checked into a source repository and updated as code
changes are made. The build system can invoke ``database.py`` to update the
database after each build.

Detokenization
--------------
Detokenization is the process of expanding a token to the string it represents
and decoding its arguments. This module provides Python and C++ detokenization
libraries.

Python
^^^^^^
To detokenize in Python, import Detokenizer from the ``pw_tokenizer`` package,
and instantiate it with paths to token databases or ELF files.

.. code-block:: python

  import pw_tokenizer

  detokenizer = pw_tokenizer.Detokenizer('path/to/database.csv', 'other/path.elf')

  def process_log_message(log_message):
    result = detokenizer.detokenize(log_message.payload)
    self._log(str(result))

The ``pw_tokenizer`` pacakge also provices the ``AutoUpdatingDetokenizer``
class, which can be used in place of the standard ``Detokenizer``. This class
monitors database files for changes and automatically reloads them when they
change. This is helpful for long-running tools that use detokenization.

C++
^^^
The C++ detokenization libraries can be used in C++ or any language that can
call into C++ with a C-linkage wrapper, such as Java or Rust. A reference
Android Java JNI is provided.

The C++ detokenization library uses binary-format token databases (created with
``--type binary``). Read a binary format database from a file or include it in
the source code. Pass the database array to ``TokenDatabase::Create``, and
construct a detokenizer.

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

Token generation: fixed length hashing at compile time
======================================================
String tokens are generated using a modified version of the x65599 hash used by
the SDBM project. All hashing is done at compile time.

In C code, strings are hashed with a preprocessor macro. For compatibility with
macros, the hash must be limited to a fixed maximum number of characters. This
value is set by ``PW_TOKENIZER_CFG_HASH_LENGTH``.

Increasing ``PW_TOKENIZER_CFG_HASH_LENGTH`` increases the compilation time for C
due to the complexity of the hashing macros. C++ macros use a constexpr
function instead of a macro, so the compilation time impact is minimal. Projects
primarily in C++ should use a large value for ``PW_TOKENIZER_CFG_HASH_LENGTH``
(perhaps even ``std::numeric_limits<size_t>::max()``).

Base64 format
=============
The tokenizer defaults to a compact binary representation of tokenized messages.
Applications may desire a textual representation of tokenized strings. This
makes it easy to use tokenized messages alongside plain text messages, but comes
at an efficiency cost.

The tokenizer module supports prefixed Base64-encoded messages: a single
character ($) followed by the Base64-encoded message. For example, the token
0xabcdef01 followed by the argument 0x05 would be encoded as ``01 ef cd ab 05``
in binary and ``$Ae/NqwU=`` in Base64.

Base64 decoding is supported in the Python detokenizer through the
``detokenize_base64`` and related functions. Base64 encoding and decoding are
not yet supprted in C++, but it is straightforward to add Base64 encoding with
any Base64 library.

.. tip::
  The detokenization tools support recursive detokenization for prefixed Base64
  text. Tokenized strings found in detokenized text are detokenized, so
  prefixed Base64 messages can be passed as ``%s`` arguments.

  For example, the message ``"$d4ffJaRn`` might be passed as the argument to a
  ``"Nested message: %s"`` string. The detokenizer would decode the message in
  two steps:

  ::

   "$alRZyuk2J3v=" → "Nested message: $d4ffJaRn" → "Nested message: Wow!"

War story: deploying tokenized logging to an existing product
=============================================================
The tokenizer module was developed to bring tokenized logging to an
in-development product. The product is complex, with several interacting
microcontrollers. It already had an established text-based logging system.
Deploying tokenization was straightforward and had substantial benefits.

**Results**
  * Log contents shrunk by over 50%, even with Base64 encoding.

    * Significant size savings for encoded logs, even using the less-efficient
      Base64 encoding required for compatibility with the existing log system.
    * Freed valueable communication bandwidth.
    * Allowed storing many more logs in crash dumps.

  * Substantial flash savings.

    * Reduced the size of 115 KB and 172 KB firmware images by over 20 KB each.
    * Shaved over 100 KB from a large 2 MB image.

  * Simpler logging code.

    * Removed CPU-heavy ``snprintf`` calls.
    * Removed complex code for forwarding log arguments to a low-priority task.

This section describes the tokenizer deployment process and highlights key
insights.

Firmware deployment
-------------------
  * In the project's logging macro, calls to the underlying logging function
    were replaced with a ``PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD``
    invocation.
  * The log level was passed as the payload argument to facilitate runtime log
    level control.
  * For this project, it was necessary to encode the log messages as text. In
    ``pw_TokenizerHandleEncodedMessageWithPayload``, the log messages were
    encoded in the $-prefixed `Base64 format`_, then dispatched as normal log
    messages.
  * Asserts were tokenized using ``PW_TOKENIZE_TO_CALLBACK``.

.. attention::
  Do not encode line numbers in tokenized strings. This results in a huge
  number of lines being added to the database, since every time code moves,
  new strings are tokenized. If line numbers are desired in a tokenized
  string, add a ``"%d"`` to the string and pass ``__LINE__`` as an argument.

Database management
-------------------
  * The token database was stored as a CSV file in the project's Git repo.
  * The token database was automatically updated as part of the build, and
    developers were expected to check in the database changes alongside their
    code changes.
  * A presubmit check verified that all strings added by a change were added to
    the token database.
  * The token database included logs and asserts for all firmware images in the
    project.
  * No strings were purged from the token database.

.. tip::
  Merge conflicts may be a frequent occurrence with an in-source database. If
  the database is in-source, make sure there is a simple script to resolve any
  merge conflicts. The script could either keep both sets of lines or discard
  local changes and regenerate the database.

Decoding tooling deployment
---------------------------
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

.. tip:: Make the tokenized logging tools simple to use.

  * Provide simple wrapper shell scripts that fill in arguments for the
    project. For example, point ``detokenize.py`` to the project's token
    databases.
  * Use ``pw_tokenizer.AutoReloadingDetokenizer`` to decode in
    continuously-running tools, so that users don't have to restart the tool
    when the token database updates.
  * Integrate detokenization everywhere it is needed. Integrating the tools
    takes just a few lines of code, and token databases can be embedded in
    APKs or binaries.

Limitations and future work
===========================

GCC bug: tokenization in template functions
-------------------------------------------
GCC incorrectly ignores the section attribute for template
`functions <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70435>`_ and
`variables <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88061>`_. Due to this
bug, tokenized strings in template functions may be emitted into ``.rodata``
instead of the special tokenized string section. This causes two problems:

  1. Tokenized strings will not be discovered by the token database tools.
  2. Tokenized strings may not be removed from the final binary.

clang does **not** have this issue! Use clang if you can.

It is possible to work around this bug in GCC. One approach would be to tag
format strings so that the database tools can find them in ``.rodata``. Then, to
remove the strings, compile two binaries: one metadata binary with all tokenized
strings and a second, final binary that removes the strings. The strings could
be removed by providing the appropriate linker flags or by removing the ``used``
attribute from the tokenized string character array declaration.

64-bit tokenization
-------------------
The Python and C++ detokenizing libraries currently assume that strings were
tokenized on a system with 32-bit ``long``, ``size_t``, ``intptr_t``, and
``ptrdiff_t``. Decoding may not work correctly for these types if a 64-bit
device performed the tokenization.

Supporting detokenization of strings tokenized on 64-bit targets would be
simple. This could be done by adding an option to switch the 32-bit types to
64-bit. The tokenizer stores the sizes of these types in the ``.tokenizer_info``
ELF section, so the sizes of these types can be verified by checking the ELF
file, if necessary.

Tokenization in headers
-----------------------
Tokenizing code in header files (inline functions or templates) may trigger
warnings such as ``-Wlto-type-mismatch`` under certain conditions. That
is because tokenization requires declaring a character array for each tokenized
string. If the tokenized string includes macros that change value, the size of
this character array changes, which means the same static variable is defined
with different sizes. It should be safe to suppress these warnings, but, when
possible, code that tokenizes strings with macros that can change value should
be moved to source files rather than headers.

Tokenized strings as ``%s`` arguments
-------------------------------------
Encoding ``%s`` string arguments is inefficient, since ``%s`` strings are
encoded 1:1, with no tokenization. It would be better to send a tokenized string
literal as an integer instead of a string argument, but this is not yet
supported.

A string token could be sent by marking an integer % argument in a way
recognized by the detokenization tools. The detokenizer would expand the
argument to the string represented by the integer.

.. code-block:: cpp

  #define PW_TOKEN_ARG "TOKEN<([\\%" PRIx32 "/])>END_TOKEN"

  constexpr uint32_t answer_token = PW_TOKENIZE_STRING("Uh, who is there");

  PW_TOKENIZE_TO_GLOBAL_HANDLER("Knock knock: " PW_TOKEN_ARG "?", answer_token);

Strings with arguments could be encoded to a buffer, but since printf strings
are null-terminated, a binary encoding would not work. These strings can be
prefixed Base64-encoded and sent as ``%s`` instead. See `Base64 format`_.

Another possibility: encode strings with arguments to a ``uint64_t`` and send
them as an integer. This would be efficient and simple, but only support a small
number of arguments.

Compatibility
=============
  * C11
  * C++17
  * Python 3

Dependencies
============
  * pw_varint module
  * pw_preprocessor module
  * pw_span module
