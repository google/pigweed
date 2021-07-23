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
     - |checkbox|

   * - 2
     - Click ``Search`` on the log toolbar
     - | The search bar appears
       | The cursor should appear after the ``/``
     - |checkbox|

   * - 3
     - Press ``Ctrl-c``
     - The search bar disappears
     - |checkbox|

   * - 4
     - Click ``Follow`` on the log toolbar
     - Logs stop following
     - |checkbox|

   * - 5
     - Click ``Table`` on the log toolbar
     - Table mode is disabled
     - |checkbox|

   * - 6
     - Click ``Wrap`` on the log toolbar
     - Line wrapping is enabled
     - |checkbox|

2. Test Log Pane Search and Filtering
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - | Click the ``Logs``
       | window title
     - Log pane is focused
     - |checkbox|

   * - 2
     - Press ``/``
     - | The search bar appears
       | The cursor should appear after the ``/``
     - |checkbox|

   * - 3
     - | Type ``lorem``
       | Press ``Enter``
     - | Logs stop following
       | The previous ``Lorem`` word is highlighted in yellow
       | All other ``Lorem`` words are highlighted in cyan
     - |checkbox|

   * - 4
     - Press ``Ctrl-f``
     - | The search bar appears
       | The cursor should appear after the ``/``
     - |checkbox|

   * - 5
     - Click ``Matcher:`` once
     - ``Matcher:STRING`` is shown
     - |checkbox|

   * - 6
     - | Type ``[=``
       | Press ``Enter``
     - All instances of ``[=`` should be highlighted
     - |checkbox|

   * - 7
     - Press ``/``
     - | The search bar appears
       | The cursor should appear after the ``/``
     - |checkbox|

   * - 8
     - Press ``Up``
     - The text ``[=`` should appear in the search input field
     - |checkbox|

   * - 9
     - Click ``Add Filter``
     - | A ``Filters`` toolbar will appear
       | showing the new filter: ``<\[= (X)>``.
       | Only log messages matching ``[=`` appear in the logs.
     - |checkbox|

   * - 10
     - | Press ``/``
       | Type ``# 1``
       | Click ``Add Filter``
     - | The ``Filters`` toolbar shows a new filter: ``<\#\ 1 (X)>``.
       | Only log messages matching both filters will appear in the logs.
     - |checkbox|

   * - 11
     - | Click the first ``(X)``
       | in the filter toolbar.
     - | The ``Filters`` toolbar shows only one filter: ``<\#\ 1 (X)>``.
       | More log messages will appear in the log window
       | Lines all end in: ``# 1.*``
     - |checkbox|

   * - 12
     - Click ``Clear Filters``
     - | The ``Filters`` toolbar will disappear.
       | All log messages will be shown in the log window.
     - |checkbox|

   * - 13
     - | Press ``/``
       | Type ``BAT``
       | Click ``Column``
     - ``Column:Module`` is shown
     - |checkbox|

   * - 14
     - | Click ``Add Filter``
     - | The Filters toolbar appears with one filter: ``<module BAT (X)>``
       | Only logs with Module matching ``BAT`` appear.
     - |checkbox|

   * - 15
     - Click ``Clear Filters``
     - | The ``Filters`` toolbar will disappear.
       | All log messages will be shown in the log window.
     - |checkbox|

   * - 16
     - | Press ``/``
       | Type ``BAT``
       | Click ``Invert``
     - ``[x] Invert`` setting is shown
     - |checkbox|

   * - 17
     - | Click ``Add Filter``
     - | The Filters toolbar appears
       | One filter is shown: ``<NOT module BAT (X)>``
       | Only logs with Modules other than ``BAT`` appear.
     - |checkbox|

3. Test Help Windows
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - Click the :guilabel:`Help > User Guide`
     - | Window appears showing the user guide with
       | RST formatting and syntax highlighting
     - |checkbox|

   * - 2
     - Press :guilabel:`q`
     - Window is hidden
     - |checkbox|

   * - 3
     - Click the :guilabel:`Help > Keyboard Shortcuts`
     - Window appears showing the keybind list
     - |checkbox|

   * - 4
     - Press :guilabel:`q`
     - Window is hidden
     - |checkbox|

   * - 5
     - Click the :guilabel:`Help > Console Test Mode Help`
     - | Window appears showing help with content
       | ``Welcome to the Pigweed Console Test Mode!``
     - |checkbox|

   * - 6
     - Press :guilabel:`q`
     - Window is hidden
     - |checkbox|

4. Add note to the commit message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Add a ``Testing:`` line to your commit message and mention the steps
executed. For example:

.. code-block:: text

   Testing: Log Pane: Manual steps 1-6

.. |checkbox| raw:: html

    <input type="checkbox">
