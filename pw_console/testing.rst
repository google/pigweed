.. _module-pw_console-testing:

=====================
Manual Test Procedure
=====================

``pw_console`` is a Terminal based user interface which is difficult to
completely test in an automated fashion. Unit tests that don't depend on the
user interface are preferred but not always possible. For those situations
manual tests should be added here to validate expected behavior.

Run in Test Mode
================

Begin each section below by running the console in test mode:

.. code-block:: shell

  pw console --test-mode

Test Sections
=============

Log Pane: Basic Actions
^^^^^^^^^^^^^^^^^^^^^^^

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

   * - 7
     - Click ``Clear`` on the log toolbar
     - | All log lines are erased
       | Follow mode is on
       | New lines start appearing
     - |checkbox|

Log Pane: Search and Filtering
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Help Windows
^^^^^^^^^^^^

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
     - Press :guilabel:`f1`
     - Window is hidden
     - |checkbox|

   * - 5
     - Click the :guilabel:`Help > Console Test Mode Help`
     - | Window appears showing help with content
       | ``Welcome to the Pigweed Console Test Mode!``
     - |checkbox|

   * - 6
     - Click the :guilabel:`[Close q]` button.
     - Window is hidden
     - |checkbox|

Window Management
^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - | Click the :guilabel:`Logs` window title
     - Log pane is focused
     - |checkbox|

   * - 2
     - | Click the menu :guilabel:`Windows > 1: Logs fake_device.1`
       | Click :guilabel:`Duplicate pane`
     - | 3 panes are visible:
       | Log pane on top
       | Repl pane in the middle
       | Log pane on the bottom
     - |checkbox|

   * - 3
     - | Click the :guilabel:`Python Input` window title
     - Python Input pane is focused
     - |checkbox|

   * - 4
     - Click the :guilabel:`View > Move Window Down`
     - | 3 panes are visible:
       | Log pane on top
       | Log pane in the middle
       | Repl pane on the bottom
     - |checkbox|

   * - 5
     - Click the :guilabel:`View > Move Window Down` again
     - | Nothing changes
       | Windows remain in the same order
     - |checkbox|

   * - 6
     - Click the :guilabel:`View > Move Window Up`
     - | 3 panes are visible:
       | Log pane on top
       | Repl pane in the middle
       | Log pane on the bottom
     - |checkbox|

   * - 7
     - | Click the menu :guilabel:`Windows > 1: Logs fake_device.1`
       | Click :guilabel:`Remove pane`
     - | 2 panes are visible:
       | Repl pane on the top
       | Log pane on bottom
     - |checkbox|

   * - 8
     - | Click the :guilabel:`Python Input`
       | window title
     - Repl pane is focused
     - |checkbox|

   * - 9
     - | Hold the keys :guilabel:`Alt- -`
       | `Alt` and `Minus`
     - Repl pane shrinks
     - |checkbox|

   * - 10
     - Hold the keys :guilabel:`Alt-=`
     - Repl pane enlarges
     - |checkbox|

   * - 11
     - | Click the menu :guilabel:`Windows > 1: Logs fake_device.1`
       | Click :guilabel:`Duplicate pane`
     - | 3 panes are visible:
       | 2 Log panes on the left
       | Repl pane on the right
     - |checkbox|

   * - 12
     - | Click the left top :guilabel:`Logs` window title
     - Log pane is focused
     - |checkbox|

   * - 13
     - Click the :guilabel:`View > Move Window Right`
     - | 3 panes are visible:
       | 1 Log panes on the left
       | 1 Log and Repl pane on the right
     - |checkbox|

   * - 14
     - | Click the menu :guilabel:`Windows > Column 2 View Modes`
       | Then click :guilabel:`[ ] Tabbed Windows`
     - | 2 panes are visible:
       | 1 Log panes on the left
       | 1 Log panes on the right
       | A tab bar on the top of the right side
       | `Logs fake_device.1` is highlighted
     - |checkbox|

   * - 15
     - | On the right side tab bar
       | Click :guilabel:`Python Repl`
     - | 2 panes are visible:
       | 1 Log pane on the left
       | 1 Repl pane on the right
       | `Python Repl` is highlighted
       | on the tab bar
     - |checkbox|

Mouse Window Resizing
^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - | Click the :guilabel:`Fake Device Logs` window
     - Log pane is focused
     - |checkbox|

   * - 2
     - | Left click and hold the :guilabel:`====` of that window
       | Drag the mouse up and down
     - This log pane is resized
     - |checkbox|

   * - 3
     - | Left click and hold the :guilabel:`====`
       | of the :guilabel:`PwConsole Debug` window
       | Drag the mouse up and down
     - | The :guilabel:`PwConsole Debug` should NOT be focused
       | The window should be resized as expected
     - |checkbox|

Copy Paste
^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - | Click the :guilabel:`Logs` window title
     - Log pane is focused
     - |checkbox|

   * - 2
     - | Click the menu
       | :guilabel:`[Edit] > Copy visible lines from active window`
       | Try pasting into a separate text editor
     - | Log lines like this:
       | ``20210729 18:31:32  INF  USB Log message ...``
       | Are in the system clipboard
     - |checkbox|

   * - 3
     - | Copy this text in your browser or
       | text editor to the system clipboard:
       | ``print('copy paste test!')``
     - | Click the :guilabel:`Python Input` window title
       | Press :guilabel:`Ctrl-v`
       | ``print('copy paste test!')`` appears
       | after the prompt.
     - |checkbox|

   * - 4
     - Press :guilabel:`Enter`
     - | This appears in Python Results:
       | ``In [1]: print('copy paste test!')``
       | ``copy paste test!``
     - |checkbox|

   * - 5
     - | Click :guilabel:`Ctrl-Alt-c -> Copy Output`
       | on the Python Results toolbar
       | Try pasting into a separate text editor
     - | The contents of the Python Results
       | are in the system clipboard.
     - |checkbox|

   * - 6
     - Click the :guilabel:`Python Results` window title
     - | Python Results is focused with cursor
       | appearing below the last line
     - |checkbox|

   * - 7
     - | Click and drag over ``copy paste text``
       | highlighting won't appear until
       | after the mouse button is released
     - | ``copy paste text`` is highlighted
     - |checkbox|

   * - 8
     - | Press :guilabel:`Ctrl-c`
       | Try pasting into a separate text editor
     - | ``copy paste text`` should appear (and is
       | in the system clipboard)
     - |checkbox|

   * - 9
     - Click the :guilabel:`Python Input` window title
     - Python Input is focused
     - |checkbox|

Incremental Stdout
^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - | Click the :guilabel:`Python Input` window title
     - Python Input pane is focused
     - |checkbox|

   * - 2
     - | Enter the following text and hit enter twice
       | ``import time``
       | ``for i in range(10):``
       | ``print(i); time.sleep(1)``
     - | ``Running...`` should appear in the python with
       | increasing integers incrementally appearing above
       | (not all at once after a delay).
     - |checkbox|

Python Input & Output
^^^^^^^^^^^^^^^^^^^^^

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
     - Click empty whitespace in the ``Python Results`` window
     - Python Results pane is focused
     - |checkbox|

   * - 3
     - Click empty whitespace in the ``Python Input`` window
     - Python Input pane is focused
     - |checkbox|

   * - 4
     - | Enter the following text and press enter to run
       | ``[i for i in __builtins__ if not i.startswith('_')]``
     - | The results should appear pretty printed
       | with each list element on it's own line:
       |
       |   >>> [i for i in __builtins__ if not i.startswith('_')]
       |   [ 'abs',
       |     'all',
       |     'any',
       |     'ascii'
       |
     - |checkbox|

   * - 5
     - | Enter the following text and press enter to run
       | ``globals()``
     - | The results should appear pretty printed
     - |checkbox|

   * - 6
     - | With the cursor over the Python Output,
       | use the mouse wheel to scroll up and down.
     - | The output window should be able to scroll all
       | the way to the beginning and end of the buffer.
     - |checkbox|

Add note to the commit message
==============================

Add a ``Testing:`` line to your commit message and mention the steps
executed. For example:

.. code-block:: text

   Testing: Log Pane Steps 1-6

.. |checkbox| raw:: html

    <input type="checkbox">
