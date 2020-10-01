.. _module-pw_bloat:

--------
pw_bloat
--------
The bloat module provides tools to generate size report cards for output
binaries using `Bloaty McBloatface <https://github.com/google/bloaty>`_ and
Pigweed's GN build system.

Bloat report cards allow tracking the memory usage of a system over time as code
changes are made and provide a breakdown of which parts of the code have the
largest size impact.

.. _bloat-howto:

Defining size reports
=====================
Size reports are defined using the GN template ``pw_size_report``. The template
requires at least two executable targets on which to perform a size diff. The
base for the size diff can be specified either globally through the top-level
``base`` argument, or individually per-binary within the ``binaries`` list.

**Arguments**

* ``title``: Title for the report card.
* ``base``: Optional default base target for all listed binaries.
* ``binaries``: List of binaries to size diff. Each binary specifies a target,
  a label for the diff, and optionally a base target that overrides the default
  base.
* ``source_filter``: Optional regex to filter labels in the diff output.
* ``full_report``: Boolean flag indicating whether to output a full report of
  all symbols in the binary, or a summary of the segment size changes. Default
  false.

.. code::

  import("$dir_pw_bloat/bloat.gni")

  executable("empty_base") {
    sources = [ "empty_main.cc" ]
  }

  exectuable("hello_world_printf") {
    sources = [ "hello_printf.cc" ]
  }

  exectuable("hello_world_iostream") {
    sources = [ "hello_iostream.cc" ]
  }

  pw_size_report("my_size_report") {
    title = "Hello world program using printf vs. iostream"
    base = ":empty_base"
    binaries = [
      {
        target = ":hello_world_printf"
        label = "Hello world using printf"
      },
      {
        target = ":hello_world_iostream"
        label = "Hello world using iostream"
      },
    ]
  }

When the size report target runs, it will print its report card to stdout.
Additionally, size report targets also generate ReST output, which is described
below.

Documentation integration
=========================
Bloat reports are easy to add to documentation files. All ``pw_size_report``
targets output a file containing a tabular report card. This file can be
imported directly into a ReST documentation file using the ``include``
directive.

For example, the ``simple_bloat_loop`` and ``simple_bloat_function`` size
reports under ``//pw_bloat/examples`` are imported into this file as follows:

.. code:: rst

  Simple bloat loop example
  ^^^^^^^^^^^^^^^^^^^^^^^^^
  .. include:: examples/simple_bloat_loop

  Simple bloat function example
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  .. include:: examples/simple_bloat_function

Resulting in this output:

Simple bloat loop example
^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: examples/simple_bloat_loop

Simple bloat function example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: examples/simple_bloat_function
