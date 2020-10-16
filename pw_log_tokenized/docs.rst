.. _module-pw_log_tokenized:

----------------
pw_log_tokenized
----------------
``pw_log_tokenized`` is a ``pw_log`` backend that tokenizes log messages using
the ``pw_tokenizer`` module. Log messages are tokenized and passed to the
``pw_tokenizer_HandleEncodedMessageWithPayload`` function. For maximum
efficiency, the log level, 16-bit tokenized module name, and flags bits are
passed through the payload argument.

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

See the documentation for ``pw_tokenizer`` for further details.

Build targets
-------------
The GN build for ``pw_log_tokenized`` has two targets: ``pw_log_tokenized`` and
``log_backend``. The ``pw_log_tokenized`` target provides the
``pw_log_tokenized/log_tokenized.h`` header. The ``log_backend`` target
implements the backend for the ``pw_log`` facade. ``pw_log_tokenized`` invokes
the ``pw_tokenizer:global_handler_with_payload`` facade, which must be
implemented by the user of ``pw_log_tokenized``.

.. note::
  The documentation for this module is currently incomplete.
