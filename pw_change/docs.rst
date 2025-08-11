.. _module-pw_change:

=========
pw_change
=========
.. pigweed-module::
   :name: pw_change

   * **Operate on Gerrit changes and Git commits**

--------
Overview
--------
``pw change`` has subcommands that operate on Gerrit changes and Git commits.

* ``pw change comments`` will retrieve comments on Gerrit changes
* ``pw change push`` will upload the current change to Gerrit
* ``pw change review`` will ask Gemini to review the commit at ``HEAD``. It does
  not evaluate the quantity of prior commits, and it ignores any uncommitted
  changes.

------------
Requirements
------------
``pw change review`` looks for a ``gemini`` executable in ``PATH``.
