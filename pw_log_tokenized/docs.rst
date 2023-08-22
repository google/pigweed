.. _module-pw_log_tokenized:

----------------
pw_log_tokenized
----------------
The ``pw_log_tokenized`` module contains utilities for tokenized logging. It
connects ``pw_log`` to ``pw_tokenizer``.

C++ backend
===========
``pw_log_tokenized`` provides a backend for ``pw_log`` that tokenizes log
messages with the ``pw_tokenizer`` module. The log level, 16-bit tokenized
module name, and flags bits are passed through the payload argument. The macro
eventually passes logs to the :c:func:`pw_log_tokenized_HandleLog` function,
which must be implemented by the application.

.. doxygenfunction:: pw_log_tokenized_HandleLog

Example implementation:

.. code-block:: cpp

   extern "C" void pw_log_tokenized_HandleLog(
       uint32_t payload, const uint8_t message[], size_t size) {
     // The metadata object provides the log level, module token, and flags.
     // These values can be recorded and used for runtime filtering.
     pw::log_tokenized::Metadata metadata(payload);

     if (metadata.level() < current_log_level) {
       return;
     }

     if (metadata.flags() & HIGH_PRIORITY_LOG != 0) {
       EmitHighPriorityLog(metadata.module(), message, size);
     } else {
       EmitLowPriorityLog(metadata.module(), message, size);
     }
   }

See the documentation for :ref:`module-pw_tokenizer` for further details.

Metadata in the format string
-----------------------------
With tokenized logging, the log format string is converted to a 32-bit token.
Regardless of how long the format string is, it's always represented by a 32-bit
token. Because of this, metadata can be packed into the tokenized string with
no cost.

``pw_log_tokenized`` uses a simple key-value format to encode metadata in a
format string. Each field starts with the ``■`` (U+25A0 "Black Square")
character, followed by the key name, the ``♦`` (U+2666 "Black Diamond Suit")
character, and then the value. The string is encoded as UTF-8. Key names are
comprised of alphanumeric ASCII characters and underscore and start with a
letter.

.. code-block::

  "■key1♦contents1■key2♦contents2■key3♦contents3"

This format makes the message easily machine parseable and human readable. It is
extremely unlikely to conflict with log message contents due to the characters
used.

``pw_log_tokenized`` uses three fields: ``msg``, ``module``, and ``file``.
Implementations may add other fields, but they will be ignored by the
``pw_log_tokenized`` tooling.

.. code-block::

  "■msg♦Hyperdrive %d set to %f■module♦engine■file♦propulsion/hyper.cc"

Using key-value pairs allows placing the fields in any order.
``pw_log_tokenized`` places the message first. This is prefered when tokenizing
C code because the tokenizer only hashes a fixed number of characters. If the
file were first, the long path might take most of the hashed characters,
increasing the odds of a collision with other strings in that file. In C++, all
characters in the string are hashed, so the order is not important.

Metadata in the tokenizer payload argument
-------------------------------------------
``pw_log_tokenized`` packs runtime-accessible metadata into a 32-bit integer
which is passed as the "payload" argument for ``pw_log_tokenizer``'s global
handler with payload facade. Packing this metadata into a single word rather
than separate arguments reduces the code size significantly.

Four items are packed into the payload argument:

- Log level -- Used for runtime log filtering by level.
- Line number -- Used to track where a log message originated.
- Log flags -- Implementation-defined log flags.
- Tokenized :c:macro:`PW_LOG_MODULE_NAME` -- Used for runtime log filtering by
  module.

Configuring metadata bit fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The number of bits to use for each metadata field is configurable through macros
in ``pw_log/config.h``. The field widths must sum to 32 bits. A field with zero
bits allocated is excluded from the log metadata.

.. c:macro:: PW_LOG_TOKENIZED_LEVEL_BITS

  Bits to allocate for the log level. Defaults to :c:macro:`PW_LOG_LEVEL_BITS`
  (3).

.. c:macro:: PW_LOG_TOKENIZED_LINE_BITS

  Bits to allocate for the line number. Defaults to 11 (up to line 2047). If the
  line number is too large to be represented by this field, line is reported as
  0.

  Including the line number can slightly increase code size. Without the line
  number, the log metadata argument is the same for all logs with the same level
  and flags. With the line number, each metadata value is unique and must be
  encoded as a separate word in the binary. Systems with extreme space
  constraints may exclude line numbers by setting this macro to 0.

  It is possible to include line numbers in tokenized log format strings, but
  that is discouraged because line numbers change whenever a file is edited.
  Passing the line number with the metadata is a lightweight way to include it.

.. c:macro:: PW_LOG_TOKENIZED_FLAG_BITS

  Bits to use for implementation-defined flags. Defaults to 2.

.. c:macro:: PW_LOG_TOKENIZED_MODULE_BITS

  Bits to use for the tokenized version of :c:macro:`PW_LOG_MODULE_NAME`.
  Defaults to 16, which gives a ~1% probability of a collision with 37 module
  names.

Creating and reading Metadata payloads
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``pw_log_tokenized`` provides a C++ class to facilitate the creation and
interpretation of packed log metadata payloads.

.. doxygenclass:: pw::log_tokenized::GenericMetadata
.. doxygentypedef:: pw::log_tokenized::Metadata

The following example shows that a ``Metadata`` object can be created from a
``uint32_t`` log metadata payload.

.. code-block:: cpp

  extern "C" void pw_log_tokenized_HandleLog(
      uint32_t payload,
      const uint8_t message[],
      size_t size_bytes) {
    pw::log_tokenized::Metadata metadata = payload;
    // Check the log level to see if this log is a crash.
    if (metadata.level() == PW_LOG_LEVEL_FATAL) {
      HandleCrash(metadata, pw::ConstByteSpan(
          reinterpret_cast<const std::byte*>(message), size_bytes));
      PW_UNREACHABLE;
    }
    // ...
  }

It's also possible to get a ``uint32_t`` representation of a ``Metadata``
object:

.. code-block:: cpp

  // Logs an explicitly created string token.
  void LogToken(uint32_t token, int level, int line_number, int module) {
    const uint32_t payload =
        log_tokenized::Metadata(
            level, module, PW_LOG_FLAGS, line_number)
            .value();
    std::array<std::byte, sizeof(token)> token_buffer =
        pw::bytes::CopyInOrder(endian::little, token);

    pw_log_tokenized_HandleLog(
        payload,
        reinterpret_cast<const uint8_t*>(token_buffer.data()),
        token_buffer.size());
  }

The binary tokenized message may be encoded in the :ref:`prefixed Base64 format
<module-pw_tokenizer-base64-format>` with the following function:

.. doxygenfunction:: PrefixedBase64Encode(span<const std::byte>)

Build targets
-------------
The GN build for ``pw_log_tokenized`` has two targets: ``pw_log_tokenized`` and
``log_backend``. The ``pw_log_tokenized`` target provides the
``pw_log_tokenized/log_tokenized.h`` header. The ``log_backend`` target
implements the backend for the ``pw_log`` facade. ``pw_log_tokenized`` invokes
the ``pw_log_tokenized:handler`` facade, which must be implemented by the user
of ``pw_log_tokenized``.

GCC has a bug resulting in section attributes of templated functions being
ignored. This in turn means that log tokenization cannot work for templated
functions, because the token database entries are lost at build time.
For more information see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70435.
If you are using GCC, the ``gcc_partially_tokenized`` target can be used as a
backend for the ``pw_log`` facade instead which tokenizes as much as possible
and uses the ``pw_log_string:handler`` for the rest using string logging.

Python package
==============
``pw_log_tokenized`` includes a Python package for decoding tokenized logs.

pw_log_tokenized
----------------
.. automodule:: pw_log_tokenized
  :members:
  :undoc-members:
