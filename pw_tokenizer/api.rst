:tocdepth: 2

.. _module-pw_tokenizer-api:

==========================
pw_tokenizer API reference
==========================
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Compress strings to shrink logs by +75%

.. _module-pw_tokenizer-api-configuration:

-------------
Configuration
-------------
.. tab-set::

   .. tab-item:: C++ / C
      :sync: cpp

      .. doxygenfile:: pw_tokenizer/config.h
         :sections: define

------------
Tokenization
------------
.. tab-set::

   .. tab-item:: C++ / C
      :sync: cpp

      .. doxygenfunction:: pw::tokenizer::EncodeArgs
      .. doxygenclass:: pw::tokenizer::EncodedMessage
         :members:
      .. doxygenfunction:: pw::tokenizer::MinEncodingBufferSizeBytes
      .. doxygendefine:: PW_TOKEN_FMT
      .. doxygendefine:: PW_TOKENIZE_FORMAT_STRING
      .. doxygendefine:: PW_TOKENIZE_FORMAT_STRING_ANY_ARG_COUNT
      .. doxygendefine:: PW_TOKENIZE_STRING
      .. doxygendefine:: PW_TOKENIZE_STRING_DOMAIN
      .. doxygendefine:: PW_TOKENIZE_STRING_DOMAIN_EXPR
      .. doxygendefine:: PW_TOKENIZE_STRING_EXPR
      .. doxygendefine:: PW_TOKENIZE_STRING_MASK
      .. doxygendefine:: PW_TOKENIZE_STRING_MASK_EXPR
      .. doxygendefine:: PW_TOKENIZE_TO_BUFFER
      .. doxygendefine:: PW_TOKENIZE_TO_BUFFER_DOMAIN
      .. doxygendefine:: PW_TOKENIZE_TO_BUFFER_MASK
      .. doxygendefine:: PW_TOKENIZER_REPLACE_FORMAT_STRING
      .. doxygendefine:: PW_TOKENIZER_ARG_TYPES
      .. doxygenfunction:: pw_tokenizer_EncodeArgs
      .. doxygenfunction:: pw_tokenizer_EncodeInt
      .. doxygenfunction:: pw_tokenizer_EncodeInt64
      .. doxygentypedef:: pw_tokenizer_Token

   .. tab-item:: Python
      :sync: py

      .. autofunction:: pw_tokenizer.encode.encode_token_and_args
      .. autofunction:: pw_tokenizer.tokens.pw_tokenizer_65599_hash

   .. tab-item:: Rust
      :sync: rs

      See `Crate pw_tokenizer </rustdoc/pw_tokenizer/>`_.

.. _module-pw_tokenizer-api-token-databases:

---------------
Token databases
---------------
.. tab-set::

   .. tab-item:: C++ / C
      :sync: cpp

      .. doxygenclass:: pw::tokenizer::TokenDatabase
         :members:

.. _module-pw_tokenizer-api-detokenization:

--------------
Detokenization
--------------
.. tab-set::

   .. tab-item:: Python
      :sync: py

      .. automodule:: pw_tokenizer.detokenize
         :members:

      .. automodule:: pw_tokenizer.proto
         :members:
