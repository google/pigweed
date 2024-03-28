.. _module-pw_presubmit-format:

=============================
pw_presubmit: Code formatting
=============================

.. admonition:: Note
   :class: warning

   https://pwbug.dev/326309165: While the ``pw format`` command interface is
   very stable, the ``pw_presubmit.format`` library is a work-in-progress
   effort to detach the implementation of ``pw format`` from
   the :ref:`module-pw_presubmit` module. Not all formatters are migrated, and
   the library API is unstable. After some of the core pieces land, this
   library will be moved to ``pw_code_format``.

.. _module-pw_presubmit-format-api:

=============
API reference
=============

Core
====
.. automodule:: pw_presubmit.format.core
   :members:
   :special-members: DiffCallback
   :noindex:

   .. autoclass:: pw_presubmit.format.core.ToolRunner
      :special-members: +__call__
      :private-members: +_run_tool

Formatters
==========
.. autoclass:: pw_presubmit.format.cpp.ClangFormatFormatter
   :members:
   :noindex:
