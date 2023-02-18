.. _module-pw_watch:

========
pw_watch
========
``pw_watch`` is similar to file system watchers found in web development
tooling. These watchers trigger a web server reload on source file change,
increasing iteration. In the embedded space, file system watchers are less
prevalent but no less useful! The Pigweed watcher module makes it easy to
instantly compile, flash, and run tests upon save.

.. figure:: doc_resources/pw_watch_test_demo2.gif
   :width: 1420
   :alt: pw watch running in fullscreen mode and displaying errors

   ``pw watch`` running in fullscreen mode and displaying errors.

--------------------------
``pw watch`` Command Usage
--------------------------
The simplest way to get started with ``pw_watch`` is to launch it from a shell
using the Pigweed environment as ``pw watch``. By default, ``pw_watch`` watches
for repository changes and triggers the default Ninja build target at out/. To
override this behavior, provide the ``-C`` argument to ``pw watch``.

.. argparse::
   :module: pw_watch.watch
   :func: get_parser
   :prog: pw watch
   :nodefaultconst:
   :nodescription:
   :noepilog:

--------
Examples
--------

Ninja
=====
Build the default target in ``out/`` using ``ninja``.

.. code-block:: sh

   pw watch -C out

Build ``python.lint`` and ``stm32f429i`` targets in ``out/`` using ``ninja``.

.. code-block:: sh

   pw watch python.lint stm32f429i

Build the ``pw_run_tests.modules`` target in the ``out/cmake/`` directory

.. code-block:: sh

   pw watch -C out/cmake pw_run_tests.modules

Build the default target in ``out/`` and ``pw_apps`` in ``out/cmake/``

.. code-block:: sh

   pw watch -C out -C out/cmake pw_apps

Build ``python.tests`` in ``out/`` and ``pw_apps`` in ``out/cmake/``

.. code-block:: sh

   pw watch python.tests -C out/cmake pw_apps

Bazel
=====
Run ``bazel build`` followed by ``bazel test`` on the target ``//...`` using the
``out-bazel/`` build directory.

.. code-block:: sh

   pw watch --run-command 'mkdir -p out-bazel' \
     -C out-bazel '//...' \
     --build-system-command out-bazel 'bazel build' \
     --build-system-command out-bazel 'bazel test'

Log Files
=========
Run two separate builds simultaneously and stream the output to separate build
log files. These log files are created:

- ``out/build.txt``: This will contain overall status messages and any sub build
  errors.
- ``out/build_out.txt``: Sub-build log only output for the ``out`` build
  directory:
- ``out/build_outbazel.txt``: Sub-build log only output for the ``outbazel``
  build directory.

.. code-block:: sh

   pw watch \
     --parallel \
     --logfile out/build.txt \
     --separate-logfiles \
     -C out default \
     -C outbazel '//...:all' \
     --build-system-command outbazel 'bazel build' \
     --build-system-command outbazel 'bazel test'

Including and Ignoring Files
============================
``pw watch`` only rebuilds when a file that is not ignored by Git changes.
Adding exclusions to a ``.gitignore`` causes watch to ignore them, even if the
files were forcibly added to a repo. By default, only files matching certain
extensions are applied, even if they're tracked by Git. The ``--patterns`` and
``--ignore-patterns`` arguments can be used to include or exclude specific
patterns. These patterns do not override Git's ignoring logic.

The ``--exclude-list`` argument can be used to exclude directories from being
watched. This decreases the number of files monitored with inotify in Linux.

Documentation Output
====================
When using ``--serve-docs``, by default the docs will be rebuilt when changed,
just like code files. However, you will need to manually reload the page in
your browser to see changes. If you install the ``httpwatcher`` Python package
into your Pigweed environment (``pip install httpwatcher``), docs pages will
automatically reload when changed.

Disable Automatic Rebuilds
==========================
``pw watch`` automatically restarts an ongoing build when files
change. This can be disabled with the ``--no-restart`` option. While running
``pw watch``, you may also press enter to immediately restart a build.

---------------------
Unit Test Integration
---------------------
Thanks to GN's understanding of the full dependency tree, only the tests
affected by a file change are run when ``pw_watch`` triggers a build. By
default, host builds using ``pw_watch`` will run unit tests. To run unit tests
on a device as part of ``pw_watch``, refer to your device's
:ref:`target documentation<docs-targets>`.

