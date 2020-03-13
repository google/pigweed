.. _chapter-pw-cli:

.. default-domain:: python

.. highlight:: sh

------
pw_cli
------
This directory contains the `pw` command line interface (CLI) that facilitates
working with Pigweed. The CLI module adds several subcommands prefixed with
`pw`, and provides a mechanism for other Pigweed modules to behave as "plugins"
and register themselves as `pw` commands as well. After activating the Pigweed
environment, these commands will be available for use.

Some examples:

.. code:: sh

  $ pw doctor
  $ pw format
  $ pw logdemo
  $ pw module-check
  $ pw presubmit
  $ pw test
  $ pw watch

To see an up-to-date list of `pw` subcommands, run ``pw --help``.

.. note::
  The documentation for this module is currently incomplete.
