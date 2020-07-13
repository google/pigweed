.. default-domain:: python

.. highlight:: sh

.. _chapter-watch:

--------
pw_watch
--------

``pw_watch`` is similar to file system watchers found in the web development
space. These watchers trigger a web server reload on source change, increasing
iteration. In the embedded space, file system watchers are less prevalent but no
less useful! The Pigweed watcher module makes it easy to instantly compile,
flash, and run tests upon save.

.. image:: doc_resources/pw_watch_on_device_demo.gif

.. note::

  ``pw_watch`` currently only works with Pigweed's GN build.

Module Usage
============

The simplest way to get started with ``pw_watch`` is to launch it from a shell
using the Pigweed environment.


.. code:: sh

  $ pw watch

By default, ``pw_watch`` will watch for repository changes and then trigger the
default Ninja build target for ``${PIGWEED_ROOT}/out``. To override this
behavior, follow ``pw watch`` with the path to the build directory optionally
followed by the Ninja targets you'd like to build:

.. code:: sh

  # Build the default target in the "out" directory.
  $ pw watch out

  # Build the "host" target in the "out" directory.
  $ pw watch out host

  # Build the "host" and "docs" targets in the "out" directory.
  $ pw watch out host docs

  # Build "host" target in "out", and "stm32f429i" target in "build_dir_2".
  $ pw watch --build-directory out host --build-directory build_dir_2 stm32f429i

The ``--patterns`` and ``--ignore_patterns`` arguments can be used to include
and exclude certain file patterns that will trigger rebuilds.

The ``--exclude_list`` argument can be used to exclude directories from
being watched by your system. This can decrease the inotify number in Linux
system.

Unit Test Integration
=====================

Thanks to GN's understanding of the full dependency tree, only the tests
affected by a file change are run when ``pw_watch`` triggers a build. By
default, host builds using ``pw_watch`` will run unit tests. To run unit tests
on a device as part of ``pw_watch``, refer to your device's
:ref:`target documentation<chapter-targets>`.
