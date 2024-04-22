.. _module-pw_web-log-viewer:

==========
Log viewer
==========
The log viewer component integrates into web tooling across frameworks like
Angular or React. You can customize the component to suit your requirements
and enable stakeholders to access logs with minimal code and hardware setup.

This guide assumes that you have ``pw_console`` set up and running.
See :ref:`module-pw_console-user_guide-start`.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/open-from-console.png
   :alt: Example of pw_console log viewer with **Open in browser** selected, no
    logs, and a pop-up with the URL to the web log viewer. The other pane shows
    logs since 'Open in Browser' is not selected.

   Press :kbd:`Shift+O` or click **Open in browser** in the log pane to start a
   websocket log server.

-----------
Filter logs
-----------
Filters by default apply to all columns.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/filter.png
   :alt: Filter example with the query 'log', the log viewer highlights and
    filters matches.

   Log viewer highlights and filters matches to 'log'.

To filter on a specific column, add the header name with a colon before the
filter. For example, the ``message`` column uses the format
``message:<filter>``.

The supported qualifiers are:

* ``severity``: Filters log level, such as ``info``, ``debug``,
  ``warning``, ``error``, and ``critical``.
* ``<column_name>``: Any column header name can be prefixed.

Filter syntax
=============
.. list-table::
   :widths: 30 30 40
   :header-rows: 1

   * - Filter with
     - | Syntax
     - | Examples

   * - Freeform text
     - | ``string``
     - | ``hello``
       | ``message:hello``

   * - Freeform text with spaces
     - | ``"string in quotes"``
     - | ``"hello world"``
       | ``message:"hello world"``

   * - Regular expressions
     - | ``/regex/``
     - | ``/^hello world$/``
       | ``message:/^hello world$/``

Logical operators
=================
The language interprets spaces as ``AND`` between conditions ``column:value``
or strings ``"a phrase"``.

.. list-table::
   :widths: 10 10 40 40
   :header-rows: 1

   * - Operator
     - | Syntax
     - | Use
     - | Example

   * - AND
     - | ``" "``
     - | Between conditions ``column:value``
       | or strings ``"a phrase"``
     - | ``severity:warn "hello world"``

   * - OR
     - | ``|``
     - | Between conditions ``column:value``
       | or strings ``"a phrase"``
     - | ``severity:warn | "hello world"``
       | ``message:hello | message:world``

   * - NOT
     - | ``!``
     - | Before condition ``column:value``
     - | ``!severity:warn``
       | ``!message:goodbye``

Parenthesis ``(`` and ``)`` dennote order of operations. Example of use is
``(message:hello | message:"hello world") !severity:error``.

---------------------------
Pause and resume autoscroll
---------------------------
The log viewer autoscrolls to the bottom as new entries appear. Autoscroll
pauses when the view scrolls up. To resume autoscroll, scroll to the bottom of
the view or press the **Jump to bottom** button.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/jump-to-bottom.png
   :alt: Autoscroll pauses when view scrolls up, **Jump to bottom** on center
    bottom of page resumes autoscroll.

------------------------
Toggle column visibility
------------------------
Column visibility changes with the checkboxes under the toggle column menu.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/toggle-fields.png
   :alt: Toggle fields button in topbar opens checkbox list of fields to toggle
    visibility.

--------------
Resize columns
--------------
Use resize handles between columns to adjust the width of content.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/column-resize.png
   :alt: Mouse cursor changes to column resize cursor between columns.

----------
Clear logs
----------
To remove logs and display new logs, click the **Clear logs** button.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/clear-logs.png
   :alt: Clear logs button in topbar removes all logs from view.

   Example of **Clear logs** button press.

----------------
Toggle word-wrap
----------------
To toggle between word wrapped and clipped context, click the **Word wrap**
button.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/word-wrap.png
   :alt: Example of word wrap button on, where longer messages become multiple
    lines in height.

   Example of **Word wrap** active.

---------
Add views
---------
To add a log view, click the **Add view** button under the kabob menu.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/more-actions.png
   :alt: Add view is found under the more actions button in the top menu.

   **Add view** in the kabob menu.

-------------
Download logs
-------------
To download a .txt file of logs click the **Download logs** button under the
kabob menu.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/more-actions.png
   :alt: Download logs is found under the more actions button in the top menu.

   **Download logs** in the kabob menu.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_web/download-file.png
   :alt: Logs save to a .txt file

   Logs save to a .txt file.
