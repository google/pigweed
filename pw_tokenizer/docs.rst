.. _module-pw_tokenizer:

============
pw_tokenizer
============
.. pigweed-module::
   :name: pw_tokenizer
   :tagline: Compress strings to shrink logs by +75%
   :status: stable
   :languages: C11, C++14, Python, TypeScript
   :code-size-impact: 50% reduction in binary log size
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli
      guides: module-pw_tokenizer-guides

Logging is critical, but developers are often forced to choose between
additional logging or saving crucial flash space. The ``pw_tokenizer`` module
helps address this by replacing printf-style strings with binary tokens during
compilation. This enables extensive logging with substantially less memory
usage.

.. note::
  This usage of the term "tokenizer" is not related to parsing! The
  module is called tokenizer because it replaces a whole string literal with an
  integer token. It does not parse strings into separate tokens.

The most common application of ``pw_tokenizer`` is binary logging, and it is
designed to integrate easily into existing logging systems. However, the
tokenizer is general purpose and can be used to tokenize any strings, with or
without printf-style arguments.

**Why tokenize strings?**

* Dramatically reduce binary size by removing string literals from binaries.
* Reduce I/O traffic, RAM, and flash usage by sending and storing compact tokens
  instead of strings. We've seen over 50% reduction in encoded log contents.
* Reduce CPU usage by replacing snprintf calls with simple tokenization code.
* Remove potentially sensitive log, assert, and other strings from binaries.

See :ref:`module-pw_tokenizer-design` for a more detailed explanation
of how ``pw_tokenizer`` works.

.. _module-pw_tokenizer-tokenized-logging-example:

--------------------------
Example: tokenized logging
--------------------------
This example demonstrates using ``pw_tokenizer`` for logging. In this example,
tokenized logging saves ~90% in binary size (41 → 4 bytes) and 70% in encoded
size (49 → 15 bytes).

.. mermaid::

   flowchart TD
     subgraph call_macro_sub [Call log macro]
     call_macro["LOG(#quot;Got battery abnormality; voltage = %d mV#quot;, voltage)"]
     end

     call_macro_sub -->
     plain_text_start([With Plain Text Logging]) -- Encoded as -->
     plain_text_encoded_sub

     subgraph plain_text_encoded_sub ["41 bytes in storage"]
     plain_text_encoded["#quot;Got battery abnormality; voltage = %d mV#quot;"]
     end

     plain_text_encoded_sub -- Transmitted as --> plain_text_transmitted_sub

     subgraph plain_text_transmitted_sub ["43 bytes over the wire"]
     plain_text_transmitted["#quot;Got battery abnormality; voltage = 3989 mV#quot;"]
     end

     style plain_text_start fill:#FFFFE0,stroke:#BDBB6B
     style plain_text_encoded_sub fill:#FBE0DD,stroke:#ED6357
     style plain_text_transmitted_sub fill:#FBE0DD,stroke:#ED6357

     plain_text_transmitted_sub -- Displayed as -->
     plain_text_displayed["#quot;Got battery abnormality; voltage = 3989 mV#quot;"]

     call_macro_sub -->
     tokenized_start([With Tokenized Logging]) -- Encoded as -->
     tokenized_encoded_sub

     subgraph tokenized_encoded_sub ["4 bytes in storage"]
     tokenized_encoded["`
     d9 28 47 8e
     or
     0x8e4728d9
     `"]
     end

     tokenized_encoded_sub -- Transmitted as --> tokenized_transmitted_sub

     subgraph tokenized_transmitted_sub ["6 bytes over the wire"]
     tokenized_transmitted["`
     **Token:** d9 28 47 8e
     **3989, as varint:** aa 3e
     `"]
     end

     style tokenized_start fill:#FFFFE0,stroke:#BDBB6B
     style tokenized_encoded_sub fill:#DEF3DE,stroke:#5AC35C
     style tokenized_transmitted_sub fill:#DEF3DE,stroke:#5AC35C

     tokenized_transmitted_sub -- Displayed as -->
     tokenized_displayed["#quot;Got battery abnormality; voltage = 3989 mV#quot;"]

.. toctree::
   :hidden:
   :maxdepth: 1

   api
   cli
   design
   guides
