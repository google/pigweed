.. _showcase-sense-tutorial-explore:

========================
2. Explore build targets
========================
.. _targets: https://bazel.build/concepts/build-ref#targets
.. _BUILD.bazel files: https://bazel.build/concepts/build-files

Throughout the Sense repository there are lots of `BUILD.bazel files`_.
Each ``BUILD.bazel`` file contains one or more `targets`_. These targets
are your entrypoints for doing lots of useful, common tasks, such as:

* Building source code
* Running unit tests
* Connecting to a device over a console
* Flashing a binary to a device

-------------
Query targets
-------------
When you're starting a new Bazel-based project, you'll need to create
your own Bazel targets. When you're ramping up on an existing Bazel
codebase, these targets are a good way to get an overview of how the
project works. Explore Sense's Bazel targets now:

.. tab-set::

   .. tab-item:: VS Code
      :sync: vsc

      #. Press :kbd:`Control+Shift+E` (:kbd:`Command+Shift+E` on macOS) to open
         the **Explorer** view.

      #. Within the **Explorer** list, expand the **Bazel Targets**
         section.

         .. admonition:: Where is this?

            Look at the bottom left of your VS Code window. The source code
            section (the one labeled **Sense**) is expanded by default so
            the **Bazel Targets** section gets pushed down to the far bottom.
            You can collapse the **Sense** section to make the **Bazel
            Targets** section easier to find.

         The **Bazel Targets** section should look like this:

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/build_target_v1.png

         .. admonition:: Troubleshooting

            * There's only a button that says **REFRESH TARGET LIST**. Click that
              button and wait 30-60 seconds. It should get populated after that.

            * The section is empty. Wait 30-60 seconds. It should get populated
              after that.

         This section provides an overview of all of the project's build
         rules. Right-clicking a rule lets you build or run that rule. You'll be
         using this UI a lot throughout the tutorial.

      #. Expand the **//apps/blinky** group.

         .. note::

            ``//`` means the root directory of your Sense repository.
            If you cloned Sense to ``~/sense/``, then ``//`` would
            be located at ``~/sense/``.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/blinky_targets_v1.png

   .. tab-item:: CLI
      :sync: cli

      #. List all Bazel targets:

         .. code-block:: console

            bazelisk query //...

      You should see a long list of targets, like this:

      .. code-block:: console

         //:clang_tidy_config
         # …
         //apps/blinky:blinky
         # …
         //device:bme688
         # …
         //modules/air_sensor:air_sensor
         # …
         //system:headers
         # …
         //targets:malloc
         # …
         //tools:air_measure
         # …

.. _hardware abstraction layer: https://en.wikipedia.org/wiki/Hardware_abstraction
.. _RP2040: https://www.raspberrypi.com/products/rp2040/
.. _RP2350: https://www.raspberrypi.com/products/rp2350/

----------------
Targets overview
----------------
Here's a quick summary of Sense's targets:

* **//apps/<app>**: Targets for ``<app>``, where ``<app>`` is a placeholder
  for a real app name like ``blinky`` or ``production``. Notice that each app
  has per-platform targets. E.g. ``:rp2040_blinky.elf`` produces a binary
  that can run on the Pico 1 (the `RP2040`_ is the microprocessor on that
  board) whereas ``rp2350_blinky.elf`` produces a binary for the Pico 2,
  which is powered by the `RP2350`_ microprocessor. ``:simulator_blinky``
  produces a binary that can run on your development host.
* **//devices**: Targets for building device drivers.
* **//modules/<module>**: Targets for building platform-agnostic
  `hardware abstraction layer`_ modules.
* **//system**: Targets for building the general middleware system
  that every application runs on top of.
* **//targets/<target>**: Targets for compiling the applications
  on specific platforms such as the RP2040 or RP2350.
* **//tools**: Targets for building and running tools that accompany
  the apps, such as the script for connecting to devices over
  :ref:`module-pw_console`.

.. _showcase-sense-tutorial-explore-summary:

-------
Summary
-------
In a Bazel-based project, pretty much all common development tasks like
building, testing, flashing, connecting to devices, and so on can be done
through Bazel targets. Bazel makes it easy to see all targets at a
glance. When onboarding onto a new project, browsing the list of targets
can be a helpful way for building a top-down intuition about how the
project works.

Next, head over to :ref:`showcase-sense-tutorial-build` to start building
binaries the Bazel way.
