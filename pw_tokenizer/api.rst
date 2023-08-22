.. _module-pw_tokenizer-api:

=============
API reference
=============
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Compress strings to shrink logs by +75%

-------
C++ / C
-------

Tokenization
============
.. doxygenfunction:: pw::tokenizer::EncodeArgs
.. doxygenclass:: pw::tokenizer::EncodedMessage
   :members:
.. doxygenfunction:: pw::tokenizer::MinEncodingBufferSizeBytes
.. doxygendefine:: PW_TOKENIZE_FORMAT_STRING
.. doxygendefine:: PW_TOKENIZE_STRING
.. doxygendefine:: PW_TOKENIZE_STRING_DOMAIN
.. doxygendefine:: PW_TOKENIZE_STRING_DOMAIN_EXPR
.. doxygendefine:: PW_TOKENIZE_STRING_EXPR
.. doxygendefine:: PW_TOKENIZE_STRING_MASK
.. doxygendefine:: PW_TOKENIZE_STRING_MASK_EXPR
.. doxygendefine:: PW_TOKENIZE_TO_BUFFER
.. doxygendefine:: PW_TOKENIZE_TO_BUFFER_DOMAIN
.. doxygendefine:: PW_TOKENIZE_TO_BUFFER_MASK
.. doxygendefine:: PW_TOKENIZER_ARG_TYPES
.. doxygenfunction:: pw_tokenizer_EncodeArgs
.. doxygentypedef:: pw_tokenizer_Token

Detokenization
==============
.. doxygenclass:: pw::tokenizer::TokenDatabase
   :members:

------
Python
------
.. autofunction:: pw_tokenizer.encode.encode_token_and_args
   :noindex:
.. automodule:: pw_tokenizer.proto
   :members:
.. autofunction:: pw_tokenizer.tokens.pw_tokenizer_65599_hash
   :noindex:

----
Rust
----
``pw_tokenizer``'s Rust API is documented in the
`pw_tokenizer crate's docs </rustdoc/pw_tokenizer>`_.
