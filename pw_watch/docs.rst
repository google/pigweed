.. _module-pw_watch:

.. rst-class:: with-subtitle

========
pw_watch
========
.. pigweed-module::
   :name: pw_watch

   * **Automatically trigger build actions when source files change**

----------
Background
----------
In the web development space, file system watchers like `nodemon
<https://www.npmjs.com/package/nodemon>`_ and `watchman
<https://facebook.github.io/watchman/>`_ are prevalent. These watchers trigger
actions when files change (such as reloading a web server), making development
much faster. In the embedded space, file system watchers are less prevalent but
no less useful!

.. _module-pw_watch-design:

------------
Our solution
------------
``pw_watch`` is similar to file system watchers found in web development
tooling but is focused around embedded development use cases. After changing
source code, ``pw_watch`` can instantly  compile, flash, and run tests.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_watch_test_demo2.gif
   :width: 1420
   :alt: ``pw watch`` running in fullscreen mode and displaying errors

   ``pw watch`` running in fullscreen mode and displaying errors.

Combined with the build which expresses the full dependency tree, only the exact
tests affected by source changes are run.

The demo below shows ``pw_watch`` building for a STMicroelectronics
STM32F429I-DISC1 development board, flashing the board with the affected test,
and verifying the test runs as expected. Once this is set up, you can attach
multiple devices to run tests in a distributed manner to reduce the time it
takes to run tests.

.. figure:: https://storage.googleapis.com/pigweed-media/pw_watch_on_device_demo.gif
   :width: 800
   :alt: pw_watch running on-device tests

.. _module-pw_watch-get-started:

-----------
Get started
-----------

Bazel
=====
To use ``watch`` in Pigweed's Bazel build, run ``./pw watch``. Arguments are
forwarded directly to ``bazel`` / ``bazelisk``.

.. code-block:: sh

   # Runs bazelisk build //... when files change.
   ./pw watch build //...

   # Runs an executable when files change.
   ./pw watch run //important:script -- -a --value 52

   # Builds //..., then runs the //foo:bar test.
   ./pw watch build //... , test //foo:bar

``./pw watch`` invokes ``bazelisk //:watch``, which is an alias of
``//pw_watch/py:bazel``. ``//pw_watch/py:bazel`` executes the generic
``//pw_watch/py:run`` tool with ``bazelisk`` as the prefix.

External projects may run the Bazel ``pw_watch``  tool directly as
``@pigweed//:watch``. Projects can also create their own workflow or
``//:watch`` alias.

GN
==
.. code-block:: bash

   cd ~/pigweed
   source activate.sh
   pw watch

The simplest way to get started with ``pw_watch`` is to launch it from a shell
using the Pigweed environment as ``pw watch``. By default, ``pw_watch`` watches
for repository changes and triggers the default Ninja build target at ``//out``.
To override this behavior, provide the ``-C`` argument to ``pw watch``.

See :ref:`module-pw_watch-guide` for more examples and
:ref:`module-pw_watch-cli` for detailed CLI usage information.

Custom commands
===============
.. automodule:: pw_watch.run

.. toctree::
   :hidden:
   :maxdepth: 1

   guide
   cli
