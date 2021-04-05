.. _module-pw_log_tokenized:

----------------
pw_log_tokenized
----------------
The ``pw_log_tokenized`` module contains utilities for tokenized logging. It
connects ``pw_log`` to ``pw_tokenizer``.

C++ backend
===========
``pw_log_tokenized`` provides a backend for ``pw_log`` that tokenizes log
messages with the ``pw_tokenizer`` module. By default, log messages are
tokenized with the ``PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD`` macro.
The log level, 16-bit tokenized module name, and flags bits are passed through
the payload argument. The macro eventually passes logs to the
``pw_tokenizer_HandleEncodedMessageWithPayload`` function, which must be
implemented by the application.

Example implementation:

.. code-block:: cpp

   extern "C" void pw_tokenizer_HandleEncodedMessageWithPayload(
       pw_tokenizer_Payload payload, const uint8_t message[], size_t size) {
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

Using a custom macro
--------------------
Applications may use their own macro instead of
``PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD`` by setting the
``PW_LOG_TOKENIZED_ENCODE_MESSAGE`` config macro. This macro should take
arguments equivalent to ``PW_TOKENIZE_TO_GLOBAL_HANDLER_WITH_PAYLOAD``:

  .. c:function:: PW_LOG_TOKENIZED_ENCODE_MESSAGE(pw_tokenizer_Payload log_metadata, const char* message, ...)

For instructions on how to implement a custom tokenization macro, see
:ref:`module-pw_tokenizer-custom-macro`.

Build targets
-------------
The GN build for ``pw_log_tokenized`` has two targets: ``pw_log_tokenized`` and
``log_backend``. The ``pw_log_tokenized`` target provides the
``pw_log_tokenized/log_tokenized.h`` header. The ``log_backend`` target
implements the backend for the ``pw_log`` facade. ``pw_log_tokenized`` invokes
the ``pw_tokenizer:global_handler_with_payload`` facade, which must be
implemented by the user of ``pw_log_tokenized``.

Python package
==============
``pw_log_tokenized`` includes a Python package for decoding tokenized logs.

pw_log_tokenized
----------------
.. automodule:: pw_log_tokenized
  :members:
  :undoc-members:
