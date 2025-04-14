.. _docs-contributing-build:

=======================
Build systems reference
=======================
This is a reference of commands that you may need when working with the build
systems of :ref:`docs-glossary-upstream`.

.. _docs-contributing-build-bazel:

-----
Bazel
-----
See also :ref:`seed-0111`, :ref:`docs-pw-style-bazel`,
:ref:`module-pw_build-bazel`, :ref:`module-pw_toolchain-bazel`,
and :ref:`docs-bazel-compatibility`.

.. _docs-contributing-build-bazel-build:

Build
=====

.. _docs-contributing-build-bazel-build-all:

Build everything
----------------
.. code-block:: console

   $ bazelisk build //...

Bazel automatically downloads and sets up all dependencies. You don't need to
manually install cross-platform toolchains, set up virtual environments, etc.

.. _docs-contributing-build-bazel-single:

Build a single target
---------------------
#. Inspect a ``BUILD.bazel`` file to find the name of the target you want to
   build. For example, in ``//pw_containers/BUILD.bazel`` the target for building
   :ref:`pw::Vector <module-pw_containers-vectors>` is called ``vector``.

#. Build that target:

   .. code-block:: console

      $ bazelisk build //pw_containers:vector

.. tip::

   The main target for a Pigweed :ref:`module <docs-glossary-module>` always
   matches the name of the module. E.g. within ``//pw_bytes/BUILD.bazel`` the
   main target is called ``pw_bytes``. You can build that target with
   ``bazelisk build //pw_bytes:pw_bytes``. When the target name matches the
   directory name, you can omit the target name and shorten the command to this:
   ``bazelisk build //pw_bytes``

.. _docs-contributing-build-bazel-platform:

Build for a specific hardware platform
--------------------------------------
.. code-block::

   $ bazelisk build --config=rp2040 //...

.. _//targets: https://cs.opensource.google/pigweed/pigweed/+/main:targets/

The value for ``--config`` should be one of the directory names listed in
`//targets`_.

.. _docs-contributing-build-bazel-watch:

Automatically rebuild when files change
---------------------------------------
.. code-block:: console

   $ bazelisk run //:watch build //...

:ref:`module-pw_watch` lets you automatically rebuild the code when files change.

.. tip::

   You can use whatever command you like after ``bazelisk run //:watch``. For
   example, if you only wanted to rebuild a single target,
   you can instead run a command like this:
   ``bazelisk run //:watch build //pw_containers:vector``

.. _docs-contributing-build-bazel-docs:

Build the docs
--------------
Simultaneously build the docs and spin up a local server so that you can
preview the docs in a web browser:

.. code-block:: console

   $ bazelisk run //docs:docs.serve

See :ref:`contrib-docs-build` for more docs-related workflows and
:ref:`docs-contrib-docs` for guidance about authoring docs.

.. _docs-contributing-build-bazel-test:

Test
====

.. _docs-contributing-build-bazel-test-all:

Run all tests
-------------
.. code-block:: console

   $ bazelisk test //...

.. _docs-contributing-build-bazel-test-single:

Run a single test
-----------------
#. Inspect a ``BUILD.bazel`` file to find the name of the
   test you want to run. For example, within ``//pw_varint/BUILD.bazel``
   there is a test called ``stream_test``.

#. Run that test:

   .. code-block:: console

      $ bazelisk test //pw_varint:stream_test

.. _docs-contributing-build-bazel-test-ondevice:

Run on-device tests
-------------------
On-device tests are only supported for the Raspberry Pi RP2040. See
:ref:`target-rp2040-upstream-tests`.

.. _docs-contributing-build-gn:

--
GN
--
See also :ref:`module-pw_build-gn`, :ref:`module-pw_toolchain-gn`,
and :ref:`docs-python-build`.

.. _docs-contributing-build-gn-bootstrap:

Bootstrap the Pigweed environment
=================================
.. tab-set::

   .. tab-item:: Bash

      .. code-block:: console

         $ . bootstrap.sh

   .. tab-item:: Fish

      .. code-block:: console

         $ . bootstrap.fish

   .. tab-item:: Windows

      .. code-block:: console

         $ bootstrap.bat

Example:

.. image:: https://storage.googleapis.com/pigweed-media/pw_env_setup_demo.gif
  :width: 800
  :alt: build example using pw watch

.. _docs-contributing-build-gn-activate:

Activate the Pigweed environment
--------------------------------
.. tab-set::

   .. tab-item:: Bash

      .. code-block:: console

         $ . activate.sh

   .. tab-item:: Fish

      .. code-block:: console

         $ . activate.fish

   .. tab-item:: Windows

      .. code-block:: console

         $ activate.bat

You don't need to :ref:`bootstrap <docs-contributing-build-gn-bootstrap>`
before every development session. You can instead re-activate your
previously bootstrapped environment, which in general is much faster.

.. _docs-contributing-build-gn-configure:

Configure the GN build
======================
.. code-block:: console

   $ gn gen out

.. _docs-contributing-build-gn-watch:

Watch
=====
.. code-block:: console

   $ pw watch

``pw watch`` automatically rebuilds the code and re-runs tests when files change.

Example:

.. image:: https://storage.googleapis.com/pigweed-media/pw_watch_build_demo.gif
   :width: 800
   :alt: build example using pw watch

Watch one target
----------------
.. code-block:: bash

   $ pw watch stm32f429i

See also :ref:`docs-targets`.

.. _docs-contributing-build-gn-build:

Build
=====

.. _docs-contributing-build-gn-build-all:

Build everything
----------------
To build everything:

.. code-block:: console

   $ ninja -C out

.. note::

   ``out`` is simply the directory the build files are saved to. Unless
   this directory is deleted or you desire to do a clean build, there's no need
   to run GN again; just rebuild using Ninja directly.

.. _b/278898014: https://issuetracker.google.com/278898014
.. _b/278906020: https://issuetracker.google.com/278906020

.. warning::

   Unless your build directory (the ``out`` in ``gn gen out``) is exactly one
   directory away from the project root directory (the Pigweed repo root in this
   case), there will be issues finding source files while debugging and while
   generating coverage reports. This is due an issue in upstream LLVM reordering
   debug and coverage path mappings. See `b/278898014`_ and `b/278906020`_.

.. _docs-contributing-build-gn-build-single:

Build one target
----------------
.. code-block:: bash

   $ ninja -C out stm32f429i

See also :ref:`docs-targets`.

.. _docs-contributing-build-gn-build-docs:

Build only the docs
-------------------
.. code-block:: bash

   $ ninja -C out docs

The generated docs are output to ``//out/docs/gen/docs/html``.

.. _docs-contributing-build-gn-build-tests:

Build tests individually
------------------------
Use ``gn outputs`` to translate a GN build step into a Ninja build step. Append
the GN path to the :ref:`target <docs-targets>` toolchain in parentheses, after
the desired build step label.

.. code-block:: console

   $ gn outputs out "//pw_status:status_test.run(//targets/host/pigweed_internal:pw_strict_host_clang_debug)"
   pw_strict_host_clang_debug/obj/pw_status/status_test.run.pw_pystamp

   $ ninja -C out pw_strict_host_clang_debug/obj/pw_status/status_test.run.pw_pystamp
   ninja: Entering directory `out'
   [4/4] ACTION //pw_status:status_test.run(//targets/host/pigweed_internal:pw_strict_host_clang_debug)

The ``.run`` following the test target name is a sub-target created as part of
the ``pw_test`` GN template. If you remove ``.run``, the test will build but
not attempt to run.

In macOS and Linux, ``xargs`` can be used to turn this into a single command:

.. code-block:: console

   $ gn outputs out "//pw_status:status_test.run(//targets/host/pigweed_internal:pw_strict_host_clang_debug)" | xargs ninja -C out

.. _docs-contributing-build-gn-tests:

Test
====

.. _docs-contributing-build-gn-tests-all:

Run all tests
-------------
:ref:`pw watch <docs-contributing-build-gn-watch>` automatically runs tests. Example:

.. image:: https://storage.googleapis.com/pigweed-media/pw_watch_test_demo.gif
  :width: 800
  :alt: example test failure using pw watch

.. _docs-contributing-build-gn-tests-manual:

Manually run an invididual test
-------------------------------
.. code-block:: console

   $ ./out/pw_strict_host_clang_debug/obj/pw_status/test/status_test

.. _docs-contributing-build-gn-tests-device:

Run tests on-device
-------------------
See :ref:`target-stm32f429i-disc1-test`.
