.. _module-pw_review:

=========
pw_review
=========
.. pigweed-module::
   :name: pw_review

   * **Use AI to review a commit**

--------
Overview
--------
``pw review`` will ask Gemini to review the commit at ``HEAD``. It does not
evaluate the quantity of prior commits, and it ignores any uncommitted changes.

------------
Requirements
------------
``pw review`` looks for a ``gemini`` executable in ``PATH``.
