.. _module-pw_tokenizer-cli:

=============
CLI reference
=============
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Cut your log sizes in half
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli

.. _module-pw_tokenizer-cli-encoding:

pw_tokenizer.encode: Encoding command line utility
==================================================
The ``pw_tokenizer.encode`` command line tool can be used to encode
format strings and optional arguments.

.. code-block:: bash

  python -m pw_tokenizer.encode [-h] FORMAT_STRING [ARG ...]

Example:

.. code-block:: text

  $ python -m pw_tokenizer.encode "There's... %d many of %s!" 2 them
        Raw input: "There's... %d many of %s!" % (2, 'them')
  Formatted input: There's... 2 many of them!
            Token: 0xb6ef8b2d
          Encoded: b'-\x8b\xef\xb6\x04\x04them' (2d 8b ef b6 04 04 74 68 65 6d) [10 bytes]
  Prefixed Base64: $LYvvtgQEdGhlbQ==

See ``--help`` for full usage details.
