.. _docs-pw-style-cli:

=========
CLI style
=========
This guide helps ensure that command-line interface (CLI) utilities in Pigweed
behave in a reasonably consistent manner. This applies to build tools, test
programs, etc.

The Pigweed CLI style guide is based on the `Command Line Interface Guidelines
<https://clig.dev/>`_ (CLIG), and mostly defers to it. The CLIG applies to
Pigweed except as noted. This document covers key guidelines and Pigweed
specifics.

As most Pigweed CLIs are not expected to be full-fledged user interfaces like
``git`` or ``docker``---but rather a collection of smaller tools---this guide
focuses on specific basics rather than broad philosophy or advanced topics.
The Pigweed CLI style guide only applies to utilities included in Pigweed
itself; projects which use Pigweed are free to conform to this, or any other
guide as they see fit.

--------
Examples
--------
The following programs demonstrate conformance to Pigweed's CLI style guide rules:

* `pw_digital_io_linux_cli
  <https://cs.opensource.google/pigweed/pigweed/+/main:pw_digital_io_linux/digital_io_cli.cc>`_

  * Note: This does not yet fully conform. See issue `#330435501
    <https://pwbug.dev/330435501>`_.

----------
Exit codes
----------
Programs must exit with a zero exit code on success and nonzero on failure.

If multiple non-zero exit codes are used, they should be clearly documented and
treated as a stable API. If appropriate, consider using :ref:`module-pw_status` codes.

.. note::
   In no case should a program return ``-1`` from ``main()``. The exit code is
   essentially a ``uint8_t`` so returning ``-1`` would result in an exit status
   of 255.

.. tip::
   Avoid exit codes 126 and above because those conflict with exit codes
   returned by some shells. This can create ambiguity when a tool is called via
   a wrapper script. See https://tldp.org/LDP/abs/html/exitcodes.html.

--------------
Program output
--------------
Following these guidelines ensures that the output of a program can be properly
used in a shell pipeline (e.g. ``pw_foo | grep foo``) or otherwise consumed by
another program or script.

See `CLIG:Guidelines Output <https://clig.dev/#output>`_.

Standard output
===============
The main output of a program should be written to *standard out* (``stdout``).

.. tab-set-code::

   .. code-block:: c++

      #include <iostream>

      int main() {
        std::cout << "foo: " << foo() << std::endl;
        std::cout << "bar: " << bar() << std::endl;
      }

   .. code-block:: c

      #include <stdio.h>

      int main() {
        printf("foo: %d\n", foo());
        printf("bar: %d\n", bar());
      }

   .. code-block:: python

      def main() -> int:
          print("foo:", foo())
          print("bar:", bar())
          return 0

Standard error
==============
Debug logs, error messages, etc. should be written to *standard error* (``stderr``).

.. tab-set-code::

   .. code-block:: c++

      #include <iostream>

      int main() {
        if (!InitFoo()) {
          std::cerr << "Error: failed to initialize foo!" << std::endl;
          return 2;
        }
        // ...
      }

   .. code-block:: c

      #include <stdio.h>

      int main() {
        if (!InitFoo()) {
          fprintf(stderr, "Error: failed to initialize foo!\n");
          return 2;
        }
        // ...
      }

   .. code-block:: python

      import sys

      def main() -> int:
          if not init_foo():
            print("Error: failed to initialize foo!", file=sys.stderr)
            return 2
          # ...

-------
Logging
-------
It is recommended to use :ref:`module-pw_log` for logging, including
``PW_LOG_DEBUG`` for debug messages, and ``PW_LOG_ERROR`` for all error
conditions.

.. warning::

   Currently there is no preconfigured ``pw_log`` backend which sends log
   messages to ``stderr``.
   See issue `#329747262 <https://pwbug.dev/329747262>`_.

   This can be achieved by using ``pw_log_basic`` and calling ``SetOutput()``
   as follows:

   .. code-block:: c++

      pw::log_basic::SetOutput([](std::string_view log) {
        std::cerr << log << std::endl;
      });

.. warning::

   Currently there is no mechanism for setting the ``pw_log`` level at runtime.
   (E.g. via ``--verbose`` or ``--quiet`` options).
   See issue `#329755001 <https://pwbug.dev/329755001>`_.

**Exceptions**:

* Informative messages which should be written to ``stderr``, but which form a
  core part of the user interface, can be written directly to ``stderr`` rather
  than via ``pw_log``. You can do this for usage text, for example.

-------------------
Arguments and flags
-------------------
See `CLIG:Guidelines Arguments and flags <https://clig.dev/#arguments-and-flags>`_.

- Prefer flags over (positional) arguments. This leads to a more verbose, but
  much more extensible interface.

  .. admonition:: **Yes**: Using flags to clarify inputs
     :class: checkmark

     .. code-block:: console

        $ pw_foo --symbols=symbols.txt --config=config.json --passes=7 \
                 --bin-out=output.bin --map-out=output.map

  .. admonition:: **No**: Using a lot of positional arguments
     :class: error

     .. code-block:: console

        $ pw_foo symbols.txt config.json 7 output.bin output.map

- Prefer subcommands (which are naturally mutually exclusive) over
  mutually exclusive flags.

  .. admonition:: **Yes**: Using subcommands to specify actions
     :class: checkmark

     .. code-block:: console

        $ pw_foo get --key abc
        $ pw_foo set --key abc

  .. admonition:: **No**: Using mutually-exclusive flags
     :class: error

     .. code-block:: console

        $ pw_foo --get-key abc
        $ pw_foo --set-key abc
        $ pw_foo --get-key abc --set-key abc  # Error

- Show usage or help text when no subcommand or arguments are provided.
  Display full help text by default unless it is longer than 24 lines, in which
  case, show abbreviated usage text. Show full help text if ``--help`` is given.
