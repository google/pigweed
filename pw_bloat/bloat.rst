.. _chapter-bloat:

.. default-domain:: cpp

.. highlight:: sh

-----
Bloat
-----
The bloat module provides tools to generate size report cards for output
binaries.

.. TODO(frolv): Explain how bloat works and how to set it up.

Documentation integration
=========================
Bloat reports are easy to add to documentation files. All ``pw_size_report``
targets output a ``.rst`` file containing a tabular report card. This file
can be imported directly into a documentation file using the ``include``
directive.

For example, the ``simple_bloat`` bloat report under ``//pw_bloat/examples``
is imported into this file as follows:

.. code:: rst

  .. include:: examples/simple_bloat.rst

Resulting in this output:

.. include:: examples/simple_bloat.rst
