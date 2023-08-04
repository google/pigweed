.. _module-pw_tokenizer-cli:

=============
CLI reference
=============
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Cut your log sizes in half

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

.. _module-pw_tokenizer-cli-detokenizing:

Detokenizing command line utilties
==================================
``pw_tokenizer`` provides two standalone command line utilities for detokenizing
Base64-encoded tokenized strings.

* ``detokenize.py`` -- Detokenizes Base64-encoded strings in files or from
  stdin.
* ``serial_detokenizer.py`` -- Detokenizes Base64-encoded strings from a
  connected serial device.

If the ``pw_tokenizer`` Python package is installed, these tools may be executed
as runnable modules. For example:

.. code-block::

   # Detokenize Base64-encoded strings in a file
   python -m pw_tokenizer.detokenize -i input_file.txt

   # Detokenize Base64-encoded strings in output from a serial device
   python -m pw_tokenizer.serial_detokenizer --device /dev/ttyACM0

See the ``--help`` options for these tools for full usage information.
