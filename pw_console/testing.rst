.. _module-pw_console-testing:

Manual Test Procedure
=====================

``pw_console`` is a Terminal based user interface which is difficult to
completely test in an automated fashion. Unit tests that don't depend on the
user interface are preferred but not always possible. For those situations
manual tests should be added here to validate expected behavior.

Testing Steps
-------------

1. Run in Test Mode
^^^^^^^^^^^^^^^^^^^

Run the console in test mode:

.. code-block:: shell

  pw console --test-mode

.. |uncheck| raw:: html

    <input type="checkbox">

2. Test the Log Pane
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅
   * - 1
     - Click the ``Logs`` window title
     - Log pane is focused
     - |uncheck|
   * - 2
     - Click ``Search`` on the log toolbar
     - The search bar appears with the cursor enabled
     - |uncheck|
   * - 3
     - Press ``Ctrl-c``
     - The search bar disappears
     - |uncheck|
   * - 4
     - Click ``Follow`` on the log toolbar
     - Logs stop following
     - |uncheck|
   * - 5
     - Click ``Table`` on the log toolbar
     - Table mode is disabled
     - |uncheck|
   * - 6
     - Click ``Wrap`` on the log toolbar
     - Line wrapping is enabled
     - |uncheck|

3. Add note to the commit message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Add a ``Testing:`` line to your commit message and mention the steps
executed. For example:

.. code-block:: text

   Testing: Log Pane: Manual steps 1-6
