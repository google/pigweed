.. _showcase-sense-tutorial-explore:

========================
2. Explore build targets
========================
.. _targets: https://bazel.build/concepts/build-ref#targets

Throughout the Sense repository there are lots of ``BUILD.bazel`` files.
Each ``BUILD.bazel`` file contains one or more `targets`_. These targets
are your entrypoints for doing lots of useful, common tasks, such as:

* Building a binary
* Running unit tests
* Connecting to a device over a console
* Flashing a binary to a device

When you're starting a new Bazel-based project, you'll need to create
your own Bazel targets. When you're ramping up on an existing Bazel
codebase, these targets are a good way to get an overview of how the
project works. Explore Sense's Bazel targets now:

.. tab-set::

   .. tab-item:: VS Code
      :sync: vsc

      #. Press :kbd:`Control+Shift+E` (:kbd:`Command+Shift+E` on macOS) to open
         the **Explorer** view.

      #. Within the **Explorer** list, expand the **Bazel Build Targets**
         section.

         .. admonition:: Where is this?

            Look at the bottom left of your VS Code window. The source code
            is expanded by default so the **Bazel Build Targets** section gets
            pushed down to the far bottom. You can collapse the source code
            section (the one labeled **Sense**) to make the **Bazel Build
            Targets** section easier to find.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/build_targets.png

         This section provides an overview of all of the project's build
         rules. Right-clicking a rule lets you build or run that rule. You'll be
         using this UI a lot throughout the tutorial.

         .. admonition:: Troubleshooting

            * **The section is empty**. Just wait a little bit. It should get
              populated after 30 seconds or so. It takes the Bazel extension
              some time to find all the Bazel targets.

         .. note::

            Although this UI is called **Bazel Build Targets** you may want to
            think of it instead as just **Bazel Targets**. This UI isn't just
            used for building. You also use it to run tests, connect to a device
            over a console, and so on.

      #. Expand the **//apps/blinky** group.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/apps_blinky.png

         .. note::

            ``//`` means the root directory of your Sense repository.
            If you cloned Sense to ``~/sense``, then ``//apps/blinky`` would
            be located at ``~/sense/apps/blinky``.

   .. tab-item:: CLI
      :sync: cli

      #. List all Bazel targets:

         .. code-block:: console

            $ bazelisk query //...

      You should see output like this:

      .. code-block:: console

         Starting local Bazel server and connecting to it...
         //:copy_clangd
         //:pw_console_config
         //:refresh_compile_commands
         //:refresh_compile_commands.check_python_version.py
         //:refresh_compile_commands.py
         //apps/blinky:blinky
         //apps/blinky:flash
         //apps/blinky:flash_rp2040
         //apps/blinky:rp2040_blinky.elf
         //apps/blinky:rp2040_console
         //apps/blinky:rp2040_example_script
         //apps/blinky:rp2040_toggle_blinky
         //apps/blinky:rp2040_webconsole
         //apps/blinky:simulator_blinky
         //apps/blinky:simulator_console
         //apps/blinky:simulator_webconsole
         # ...

.. _hardware abstraction layer: https://en.wikipedia.org/wiki/Hardware_abstraction

Here's a quick summary of Sense's targets:

* **//apps/<app>**: Targets for building ``<app>``,
  flashing ``<app>`` to a Pico, simulating ``<app>``
  on your development host, and communicating with a device
  running ``<app>`` over a console. We're using ``<app>`` as a placeholder
  here, the real app names are ``blinky``, ``production``, and so on.
  Note that there are different targets for building apps for different
  platforms, e.g. ``:rp2040_blinky.elf`` for building the binary that runs
  ``blinky`` on a Raspberry Pi RP2040 versus ``:simulator_blinky``
  for the binary that runs on your development host.
* **//devices**: Targets for building device drivers.
* **//modules/<module>**: Targets for building platform-agnostic
  `hardware abstraction layer`_ modules.
* **//system**: Targets for building the general middleware system
  that every application runs on top of.
* **//targets/<target>**: Targets for compiling the applications
  for specific platforms such as the Raspberry Pi RP2040 MCU or
  your development host.
* **//tools**: Targets for building and running tools that accompany
  the apps, such as the script for connecting to devices over
  :ref:`module-pw_console`.

.. _showcase-sense-tutorial-explore-summary:

-------
Summary
-------
In a Bazel-based project, pretty much all common development tasks like
building, flashing, connecting to devices, and so on are usually done
through Bazel targets. Bazel makes it easy to see all targets at a
glance. When onboarding onto a new project, browsing the list of targets
can be a helpful way for building a top-down intuition about how the
project works.

Next, head over to :ref:`showcase-sense-tutorial-build` to start building
binaries the Bazel way.
