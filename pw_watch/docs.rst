.. _module-pw_watch:

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

  ``pw_watch`` currently only works with Pigweed's GN and CMake builds.

Module Usage
============
The simplest way to get started with ``pw_watch`` is to launch it from a shell
using the Pigweed environment as ``pw watch``. By default, ``pw_watch`` watches
for repository changes and triggers the default Ninja build target for an
automatically located build directory (typically ``$PW_ROOT/out``). To override
this behavior, provide the ``-C`` argument to ``pw watch``.

.. code:: sh

  # Find a build directory and build the default target
  pw watch

  # Find a build directory and build the stm32f429i target
  pw watch python.lint stm32f429i

  # Build pw_run_tests.modules in the out/cmake directory
  pw watch -C out/cmake pw_run_tests.modules

  # Build the default target in out/ and pw_apps in out/cmake
  pw watch -C out -C out/cmake pw_apps

  # Find a directory and build python.tests, and build pw_apps in out/cmake
  pw watch python.tests -C out/cmake pw_apps

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
:ref:`target documentation<docs-targets>`.
