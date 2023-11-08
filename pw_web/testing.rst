.. _module-pw_web-testing:

=====================
Manual Test Procedure
=====================
``pw_web`` is a web based log viewer and the manual tests here are intended
to address situations where automated tests are not able to cover.

Test Sections
=============

Log View Controls
^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 5 45 45 5
   :header-rows: 1

   * - #
     - Test Action
     - Expected Result
     - ✅

   * - 1
     - | Input bar is empty
       | Press the :guilabel:`clear logs` button  (trash can with lines)
     - | Logs are cleared and entries after time of button press are addded.
     - |checkbox|

   * - 2
     - | Input bar has a single word filter
       | Press the :guilabel:`clear logs` button  (trash can with lines)
     - | Logs are cleared and filtered entries after time of button press are addded.
     - |checkbox|

Add note to the commit message
==============================
Add a ``Testing:`` line to your commit message and mention the steps
executed. For example:

.. code-block:: text

   Testing: Log Pane Steps 1-6

.. |checkbox| raw:: html

    <input type="checkbox">
