.. default-domain:: cpp

.. highlight:: sh

.. _chapter-pw-log-tokenized:

----------------
pw_log_tokenized
----------------
``pw_log_tokenized`` is a ``pw_log`` backend that tokenizes log messages using
the ``pw_tokenizer`` module. Log messages are tokenized and passed to the
``pw_TokenizerHandleEncodedMessageWithPayload`` function. For maximum
efficiency, the log level, 16-bit tokenized module name, and flags bits are
passed through the payload argument.

Example implementation:

.. code-block:: cpp

   extern "C" void pw_TokenizerHandleEncodedMessageWithPayload(
       pw_TokenizerPayload payload, const uint8_t message[], size_t size) {
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

See the documentation for ``pw_tokenizer`` for further details.

.. note::
  The documentation for this module is currently incomplete.
