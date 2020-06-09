.. _chapter-pw-cli:

.. default-domain:: python

.. highlight:: sh

------
pw_cli
------
This directory contains the ``pw`` command line interface (CLI) that facilitates
working with Pigweed. The CLI module adds several subcommands prefixed with
``pw``, and provides a mechanism for other Pigweed modules to behave as
"plugins" and register themselves as ``pw`` commands as well. After activating
the Pigweed environment, these commands will be available for use.

``pw`` includes the following commands by default:

.. code-block:: text

  doctor        Check that the environment is set up correctly for Pigweed.
  format        Check and fix formatting for source files.
  help          Display detailed information about pw commands.
  logdemo       Show how logs look at various levels.
  module-check  Check that a module matches Pigweed's module guidelines.
  test          Run Pigweed unit tests built using GN.
  watch         Watch files for changes and rebuild.

To see an up-to-date list of ``pw`` subcommands, run ``pw --help``.

Invoking  ``pw``
================
``pw`` subcommands are invoked by providing the command name. Arguments prior to
the command are interpreted by ``pw`` itself; all arguments after the command
name are interpreted by the command.

Here are some example invocations of ``pw``:

.. code-block:: text

  # Run the doctor command
  $ pw doctor

  # Run format --fix with debug-level logs
  $ pw --loglevel debug format --fix

  # Display help for the pw command
  $ pw -h watch

  # Display help for the watch command
  $ pw watch -h

Registering ``pw`` plugins
==========================
Projects can register their own Python scripts as ``pw`` commands. ``pw``
plugins are registered by providing the command name, module, and function in a
``PW_PLUGINS`` file. ``PW_PLUGINS`` files can add new commands or override
built-in commands. Since they are accessed by module name, plugins must be
defined in Python packages that are installed in the Pigweed virtual
environment.

Plugin registrations in a ``PW_PLUGINS`` file apply to the their directory and
all subdirectories, similarly to configuration files like ``.clang-format``.
Registered plugins appear as commands in the ``pw`` tool when ``pw`` is run from
those directories.

Projects that wish to register commands might place a ``PW_PLUGINS`` file in the
root of their repo. Multiple ``PW_PLUGINS`` files may be applied, but the ``pw``
tool gives precedence to a ``PW_PLUGINS`` file in the current working directory
or the nearest parent directory.

PW_PLUGINS file format
----------------------
``PW_PLUGINS`` contains one plugin entry per line in the following format:

.. code-block:: python

  # Lines that start with a # are ignored.
  <command name> <Python module> <function>

The following example registers three commands:

.. code-block:: python

  # Register the presubmit script as pw presubmit
  presubmit my_cool_project.tools run_presubmit

  # Override the pw test command with a custom version
  test my_cool_project.testing run_test

  # Add a custom command
  flash my_cool_project.flash main

Defining a plugin function
--------------------------
Any function without required arguments may be used as a plugin function. The
function should return an int, which the ``pw`` uses as the exit code. The
``pw`` tool uses the function docstring as the help string for the command.

Typically, ``pw`` commands parse their arguments with the ``argparse`` module.
``pw`` sets ``sys.argv`` so it contains only the arguments for the plugin,
so plugins can behave the same whether they are executed independently or
through ``pw``.

Example
^^^^^^^
This example shows a function that is registered as a ``pw`` plugin.

.. code-block:: python

  # my_package/my_module.py

  def _do_something(device):
      ...

  def main() -> int:
      """Do something to a connected device."""

      parser = argparse.ArgumentParser(description=__doc__)
      parser.add_argument('--device', help='Set which device to target')
      return _do_something(**vars(parser.parse_args()))


  if __name__ == '__main__':
      logging.basicConfig(format='%(message)s', level=logging.INFO)
      sys.exit(main())

This plugin is registered in a ``PW_PLUGINS`` file in the current working
directory or a parent of it.

.. code-block:: python

  # Register my_commmand
  my_command my_package.my_module main

The function is now available through the ``pw`` command, and will be listed in
``pw``'s help. Arguments after the command name are passed to the plugin.

.. code-block:: text

  $ pw

   ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
    ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
    ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
    ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
    ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀

  usage: pw [-h] [-C DIRECTORY] [-l LOGLEVEL] [--no-banner] [command] ...

  The Pigweed command line interface (CLI).

  ...

  supported commands:
    doctor        Check that the environment is set up correctly for Pigweed.
    format        Check and fix formatting for source files.
    help          Display detailed information about pw commands.
    ...
    my_command    Do something to a connected device.

  $ pw my_command -h

   ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
    ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
    ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
    ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
    ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀

  usage: pw my_command [-h] [--device DEVICE]

  Do something to a connected device.

  optional arguments:
    -h, --help       show this help message and exit
    --device DEVICE  Set which device to target
