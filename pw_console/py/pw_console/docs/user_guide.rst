.. _module-pw_console-user_guide:

User Guide
==========

.. seealso::

   This guide can be viewed online at: https://pigweed.dev/pw_console/

The Pigweed Console provides a Python repl (read eval print loop) and log viewer
in a single-window terminal based interface.

.. contents::
   :local:


Starting the Console
--------------------

::

  pw rpc -s localhost:33000 --proto-globs pw_rpc/echo.proto


Exiting
~~~~~~~

1.  Click the :guilabel:`[File]` menu and then :guilabel:`Exit`.
2.  Type :guilabel:`quit` or :guilabel:`exit` in the Python Input window.
3.  The keyboard shortcut :guilabel:`Ctrl-W` also quits.


Interface Layout
----------------

On startup the console will display multiple windows one on top of the other.

::

  +-----------------------------------------------------+
  | [File] [View] [Window] [Help]       Pigweed Console |
  +=====================================================+
  |                                                     |
  |                                                     |
  |                                                     |
  | Log Window                                          |
  +=====================================================+
  |                                                     |
  |                                                     |
  | Python Results                                      |
  +-----------------------------------------------------+
  |                                                     |
  | Python Input                                        |
  +-----------------------------------------------------+


Navigation
----------

All menus, windows, and toolbar buttons can be clicked on. Scrolling with the
mouse wheel should work too. This requires that your terminal is able to send
mouse events.


Main Menu Navigation with the Keyboard
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

============================================  =====================
Function                                      Keys
============================================  =====================
Move focus between all active UI elements     :guilabel:`Shift-Tab`

Move focus between windows and the main menu  :guilabel:`Ctrl-Up`
                                              :guilabel:`Ctrl-Down`

Move selection in the main menu               :guilabel:`Up`
                                              :guilabel:`Down`
                                              :guilabel:`Left`
                                              :guilabel:`Right`
============================================  =====================


Toolbars
~~~~~~~~

Log toolbar functions are clickable. You can also press the keyboard
shortcut highlighted in blue:

::

        / → Search  f → [✓] Follow  t → [✓] Table  w → [ ] Wrap  C → Clear


Log Window
~~~~~~~~~~

Log Window Scrolling
^^^^^^^^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Scroll logs up                                :guilabel:`Mouse Wheel Up`
                                              :guilabel:`Up`
                                              :guilabel:`k`

Scroll logs down                              :guilabel:`Mouse Wheel Down`
                                              :guilabel:`Down`
                                              :guilabel:`j`

Scroll logs up one page                       :guilabel:`PageUp`
Scroll logs down one page                     :guilabel:`PageDown`
Jump to the beginning                         :guilabel:`g`
Jump to the end                               :guilabel:`G`

Horizontal scroll left or right               :guilabel:`Left`
                                              :guilabel:`Right`

Horizontal scroll to the beginning            :guilabel:`Home`
                                              :guilabel:`0`
                                              :guilabel:`^`
============================================  =====================

Log Window View Options
^^^^^^^^^^^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Toggle line following.                        :guilabel:`f`
Toggle table view.                            :guilabel:`t`
Toggle line wrapping.                         :guilabel:`w`
Clear log pane history.                       :guilabel:`C`
============================================  =====================

Log Window Management
^^^^^^^^^^^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Duplicate this log pane.                      :guilabel:`Insert`
Remove log pane.                              :guilabel:`Delete`
============================================  =====================

Log Searching
^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Open the search bar                           :guilabel:`/`
                                              :guilabel:`Ctrl-f`
Navigate search term history                  :guilabel:`Up`
                                              :guilabel:`Down`
Start the search and highlight matches        :guilabel:`Enter`
Close the search bar without searching        :guilabel:`Ctrl-c`
============================================  =====================

Here is a view of the search bar:

::

  +-------------------------------------------------------------------------------+
  |           Enter → Search  Ctrl-Alt-f → Add Filter  Ctrl-Alt-r → Clear Filters |
  |  Search   Ctrl-t → Column:All  Ctrl-v → [ ] Invert  Ctrl-n → Matcher:REGEX    |
  | /                                                                             |
  +-------------------------------------------------------------------------------+

Across the top are various functions with keyboard shortcuts listed. Each of
these are clickable with the mouse. The second line shows configurable search
parameters.

**Search Parameters**

- ``Column:All`` Change the part of the log message to match on. For example:
  ``All``, ``Message`` or any extra metadata column.

- ``Invert`` match. Find lines that don't match the entered text.

- ``Matcher``: How the search input should be interpreted.

    - ``REGEX``: Treat input text as a regex.

    - ``STRING``: Treat input as a plain string. Any regex characters will be
      escaped when search is performed.

    - ``FUZZY``: input text is split on spaces using the ``.*`` regex. For
      example if you search for ``idle run`` the resulting search regex used
      under the hood is ``(idle)(.*?)(run)``. This would match both of these
      lines:

      .. code-block:: text

         Idle task is running
         Idle thread is running

**Active Search Shortcuts**

When a search is started the bar will close, log follow mode is disabled and all
matches will be highlighted.  At this point a few extra keyboard shortcuts are
available.

============================================  =====================
Function                                      Keys
============================================  =====================
Move to the next search result                :guilabel:`n`
                                              :guilabel:`Ctrl-g`
                                              :guilabel:`Ctrl-s`
Move to the previous search result            :guilabel:`N`
                                              :guilabel:`Ctrl-r`
Removes search highlighting                   :guilabel:`Ctrl-l`
Creates a filter using the active search      :guilabel:`Ctrl-Alt-f`
Deletes all active filters.                   :guilabel:`Ctrl-Alt-r`
============================================  =====================


Log Filtering
^^^^^^^^^^^^^

Log filtering allows you to limit what log lines appear in any given log
window. Filters can be added from the currently active search or directly in the
search bar.

- With the search bar **open**:

  Type something to search for then press :guilabel:`Ctrl-Alt-f` or click on
  :guilabel:`Add Filter`.

- With the search bar **closed**:

  Press :guilabel:`Ctrl-Alt-f` to use the current search term as a filter.

When a filter is active the ``Filters`` toolbar will appear at the bottom of the
log window. For example, here are some logs with one active filter for
``lorem ipsum``.

::

  +------------------------------------------------------------------------------+
  | Time               Lvl  Module  Message                                      |
  +------------------------------------------------------------------------------+
  | 20210722 15:38:14  INF  APP     Log message # 270 Lorem ipsum dolor sit amet |
  | 20210722 15:38:24  INF  APP     Log message # 280 Lorem ipsum dolor sit amet |
  | 20210722 15:38:34  INF  APP     Log message # 290 Lorem ipsum dolor sit amet |
  | 20210722 15:38:44  INF  APP     Log message # 300 Lorem ipsum dolor sit amet |
  | 20210722 15:38:54  INF  APP     Log message # 310 Lorem ipsum dolor sit amet |
  | 20210722 15:39:04  INF  APP     Log message # 320 Lorem ipsum dolor sit amet |
  +------------------------------------------------------------------------------+
  |  Filters   <lorem ipsum (X)>  Ctrl-Alt-r → Clear Filters                     |
  +------------------------------------------------------------------------------+
  |   Logs   / → Search  f → [✓] Follow  t → [✓] Table  w → [ ] Wrap  C → Clear  |
  +------------------------------------------------------------------------------+

**Stacking Filters**

Adding a second filter on the above logs for ``# 2`` would update the filter
toolbar to show:

::

  +------------------------------------------------------------------------------+
  | Time               Lvl  Module  Message                                      |
  +------------------------------------------------------------------------------+
  |                                                                              |
  |                                                                              |
  |                                                                              |
  | 20210722 15:38:14  INF  APP     Log message # 270 Lorem ipsum dolor sit amet |
  | 20210722 15:38:24  INF  APP     Log message # 280 Lorem ipsum dolor sit amet |
  | 20210722 15:38:34  INF  APP     Log message # 290 Lorem ipsum dolor sit amet |
  +------------------------------------------------------------------------------+
  |  Filters   <lorem ipsum (X)>  <# 2 (X)>  Ctrl-Alt-r → Clear Filters          |
  +------------------------------------------------------------------------------+
  |   Logs   / → Search  f → [✓] Follow  t → [✓] Table  w → [ ] Wrap  C → Clear  |
  +------------------------------------------------------------------------------+

Any filter listed in the Filters toolbar and can be individually removed by
clicking on the red ``(X)`` text.


Python Window
~~~~~~~~~~~~~


Running Code in the Python Repl
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Type code and hit :guilabel:`Enter` to run.
-  If multiple lines are used, move the cursor to the end and press
   :guilabel:`Enter` twice.
-  :guilabel:`Up` / :guilabel:`Down` Navigate command history
-  :guilabel:`Ctrl-r` Start reverse history searching
-  :guilabel:`Ctrl-c` Erase the input buffer

   -  If the input buffer is empty:
      :guilabel:`Ctrl-c` cancels any currently running Python commands.

-  :guilabel:`F2` Open the python repl settings (from
   `ptpython <https://github.com/prompt-toolkit/ptpython>`__). This
   works best in vertical split mode.

   -  To exit: hit :guilabel:`F2` again.
   -  Navigate options with the arrow keys, Enter will close the menu.

-  :guilabel:`F3` Open the python repl history (from
   `ptpython <https://github.com/prompt-toolkit/ptpython>`__).

   -  To exit: hit :guilabel:`F3` again.
   -  Left side shows previously entered commands
   -  Use arrow keys to navigate.
   -  :guilabel:`Space` to select as many lines you want to use

      -  Selected lines will be appended to the right side.

   -  :guilabel:`Enter` to accept the right side text, this will be inserted
      into the repl.


Copy & Pasting
~~~~~~~~~~~~~~

Copying Text
^^^^^^^^^^^^

At the moment there is no built-in copy/paste function. As a workaround use your
terminal built in selection:

- **Linux**

  - Holding :guilabel:`Shift` and dragging the mouse in most terminals.

- **Mac**

  - **Apple Terminal**:

    Hold :guilabel:`Fn` and drag the mouse

  - **iTerm2**:

    Hold :guilabel:`Cmd+Option` and drag the mouse

- **Windows**

  - **Git CMD** (included in `Git for Windows <https://git-scm.com/downloads>`__)

    1. Click on the Git window icon in the upper left of the title bar
    2. Click ``Edit`` then ``Mark``
    3. Drag the mouse to select text and press Enter to copy.

  - **Windows Terminal**

    1. Hold :guilabel:`Shift` and drag the mouse to select text
    2. Press :guilabel:`Ctrl-Shift-C` to copy.

Pasting Text
^^^^^^^^^^^^

Currently you must paste text using your terminal emulator's paste
function. How to do this depends on what terminal you are using and on
which OS. Here's how on various platforms:

- **Linux**

  - **XTerm**

    :guilabel:`Shift-Insert` pastes text

  - **Gnome Terminal**

    :guilabel:`Ctrl-Shift-V` pastes text

- **Windows**

  - **Git CMD** (included in `Git for Windows <https://git-scm.com/downloads>`__)

    1. Click on the Git icon in the upper left of the windows title bar and open
       ``Properties``.
    2. Checkmark the option ``Use Ctrl+Shift+C/V as Copy Paste`` and hit ok.
    3. Then use :guilabel:`Ctrl-Shift-V` to paste.

  - **Windows Terminal**

   -  :guilabel:`Ctrl-Shift-V` pastes text.
   -  :guilabel:`Shift-RightClick` also pastes text.


Window Management
~~~~~~~~~~~~~~~~~

Any window can be hidden by clicking the checkbox in the
:guilabel:`[x] Show Window` submenu

============================================  =====================
Function                                      Keys
============================================  =====================
Enlarge the currently focused window          :guilabel:`Ctrl-j`
Shrink the currently focused window           :guilabel:`Ctrl-k`
Reset window sizes                            :guilabel:`Ctrl-u`

Move current window up                        :guilabel:`Ctrl-Alt-k`
Move current window down                      :guilabel:`Ctrl-Alt-j`

Use vertical window splitting                 :guilabel:`F4`
============================================  =====================


Color Depth
-----------

Some terminals support full 24-bit color. By default pw console will try
to use 256 colors.

To force a particular color depth: set one of these environment
variables before launching the console.

::

   # 1 bit | Black and white
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_1_BIT
   # 4 bit | ANSI colors
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_4_BIT
   # 8 bit | 256 colors
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_8_BIT
   # 24 bit | True colors
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_24_BIT


Known Issues
------------


Python Repl Window
~~~~~~~~~~~~~~~~~~

- Any ``print()`` commands entered in the repl will not appear until the code
  being run is completed. This is a high priority issue:
  https://bugs.chromium.org/p/pigweed/issues/detail?id=407


Log Window
~~~~~~~~~~

- Rendering for log lines that include ``\n`` characters is broken and hidden if
  Table view is turned on.

- Tab character rendering will not work in the log pane view. They will
  appear as ``^I`` since prompt_toolkit can't render them. See this issue for details:
  https://github.com/prompt-toolkit/python-prompt-toolkit/issues/556


General
~~~~~~~

-  Mouse click and colors don't work if using Windows cmd.exe. Please
   use the newer Windows Terminal app instead: https://github.com/microsoft/terminal


Upcoming Features
-----------------

For upcoming features see the Pigweed Console Bug Hotlist at:
https://bugs.chromium.org/u/542633886/hotlists/Console


Feature Requests
~~~~~~~~~~~~~~~~

Create a feature request bugs using this template:
https://bugs.chromium.org/p/pigweed/issues/entry?owner=tonymd@google.com&labels=Type-Enhancement,Priority-Medium&summary=pw_console
