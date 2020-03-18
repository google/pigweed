.. default-domain:: cpp

.. highlight:: sh

.. _chapter-module:

---------
pw_module
---------
The ``pw_module`` module contains tools for managing Pigweed modules.
For information on the structure of a Pigweed module, refer to
:ref:`chapter-module-guide`

Commands
--------

.. _chapter-module-module-check:

``pw module-check``
^^^^^^^^^^^^^^^^^^^
The ``pw module-check`` command exists to ensure that your module conforms to
the Pigweed module norms.

For example, at time of writing ``pw module-check pw_module`` is not passing
its own lint:

.. code-block:: none

  $ pw module-check pw_module

   ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
    ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
    ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
    ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
    ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀

  20191205 17:05:19 INF Checking module: pw_module
  20191205 17:05:19 ERR PWCK005: Missing ReST documentation; need at least e.g. "docs.rst"
  20191205 17:05:19 ERR FAIL: Found errors when checking module pw_module


