.. _module-pw_watch-guide:

=====================
pw_watch how-to guide
=====================
.. pigweed-module-subpage::
   :name: pw_watch
   :tagline: Embedded development file system watcher

This guide shows you how to do common ``pw_watch`` tasks.

See :ref:`docs-build-system` for an overview of Pigweed's approach to build
systems.

-------------------------------
Set up your Pigweed environment
-------------------------------
See :ref:`activate-pigweed-environment` if you see an error like this:

.. code-block:: sh

   pw watch
   bash: pw: command not found

-----
Ninja
-----
This section contains common use cases for :ref:`docs-build-system-gn`
users.

.. _module-pw_watch-guide-ninja-custom-dirs:

Set up a custom build directory
-------------------------------

Before running any command that uses a custom build directory, you need to
run ``gn gen <dir>``, where ``<dir>`` is a placeholder for the name of your
custom build directory.

For example, before running this command:

.. code-block:: sh

   pw watch -C out2

You need to run this command:

.. code-block:: sh

   gn gen out2

Build the default target and use the default build directory
------------------------------------------------------------
.. code-block:: sh

   pw watch

The default build directory is ``out``.

Customize the build directory
-----------------------------
This section assumes you have completed
:ref:`module-pw_watch-guide-ninja-custom-dirs`.

.. code-block:: sh

   pw watch -C out2

This builds the default target in ``out2``.

Build two targets
-----------------
.. code-block:: sh

   pw watch stm32f429i python.lint

The ``stm32f429i`` and ``python.lint`` targets are both built in the default
build directory (``out``).

Build the same target in different build directories
----------------------------------------------------
This section assumes you have completed
:ref:`module-pw_watch-guide-ninja-custom-dirs`.

.. code-block:: sh

   pw watch -C out1 -C out2

This example builds the default target in both ``out1`` and ``out2``.

Build different targets in different build directories
------------------------------------------------------
This section assumes you have completed
:ref:`module-pw_watch-guide-ninja-custom-dirs`.

.. code-block:: sh

   pw watch stm32f429i -C out2 python.lint

The ``stm32f429i`` target is built in the default build directory (``out``).
The ``python.lint`` target is built in the custom build directory (``out2``).

Unit test integration
---------------------
Thanks to GN's understanding of the full dependency tree, only the tests
affected by a file change are run when ``pw_watch`` triggers a build. By
default, host builds using ``pw_watch`` will run unit tests. To run unit tests
on a device as part of ``pw_watch``, refer to your device's
:ref:`target documentation<docs-targets>`.

----------------------------
Build-system-agnostic guides
----------------------------
This section discusses general use cases that all apply to all ``pw watch``
usage. In other words, these use cases are not affected by whether you're
using GN, Bazel, and so on.

Ignore files
------------
``pw watch`` only rebuilds when a file that is not ignored by Git changes.
Adding exclusions to a ``.gitignore`` causes ``pw watch`` to ignore them, even
if the files were forcibly added to a repo. By default, only files matching
certain extensions are applied, even if they're tracked by Git. The
``--patterns`` and ``--ignore-patterns`` arguments can be used to include or
exclude specific patterns. These patterns do not override Git's ignoring logic.

The ``--exclude-list`` argument can be used to exclude directories from being
watched. This decreases the number of files monitored with ``inotify`` in Linux.

Automatically reload docs
-------------------------
When using ``--serve-docs``, by default the docs will be rebuilt when changed,
just like code files. However, you will need to manually reload the page in
your browser to see changes.

Disable automatic rebuilds
--------------------------
``pw watch`` automatically restarts an ongoing build when files change. This
can be disabled with the ``--no-restart`` option. While running ``pw watch``,
you may also press :kbd:`Enter` to immediately restart a build.
