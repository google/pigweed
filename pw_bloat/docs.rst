.. default-domain:: cpp

.. highlight:: sh

.. _chapter-bloat:

--------
pw_bloat
--------
The bloat module provides tools to generate size report cards for output
binaries.

.. _bloat-howto:

Defining size reports
=====================

.. TODO(frolv): Explain how bloat works and how to set it up.

Documentation integration
=========================
Bloat reports are easy to add to documentation files. All ``pw_size_report``
targets output a ``.rst`` file containing a tabular report card. This file
can be imported directly into a documentation file using the ``include``
directive.

For example, the ``simple_bloat_loop`` and ``simple_bloat_function`` size
reports under ``//pw_bloat/examples`` are imported into this file as follows:

.. code:: rst

  Simple bloat loop example
  ^^^^^^^^^^^^^^^^^^^^^^^^^
  .. include:: examples/simple_bloat_loop.rst

  Simple bloat function example
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  .. include:: examples/simple_bloat_function.rst

Resulting in this output:

Simple bloat loop example
^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: examples/simple_bloat_loop.rst

Simple bloat function example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: examples/simple_bloat_function.rst
