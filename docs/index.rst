.. _docs-root:

=======
Pigweed
=======
*Modern software development for embedded systems*

.. raw:: html

   <!-- Add a little space between the H2 and the cards. This is an edge case
        that only occurs on the homepage so it's best to just hack the fix
        locally here. -->
   <br>

.. grid:: 1

   .. grid-item-card:: :octicon:`rocket` Get started
      :link: docs-get-started
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Create a new project, integrate Pigweed into an existing project, browse
      example apps, or contribute to upstream Pigweed

----------------
What is Pigweed?
----------------
.. raw:: html

   <!-- Add a little space between the H2 and the cards. This is an edge case
        that only occurs on the homepage so it's best to just hack the fix
        locally here. -->
   <br>

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Libraries
      :link: docs-concepts-embedded-development-libraries
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Over 150 libraries ("modules") enabling you to use modern C++ and
      software development best practices without compromising performance,
      code size, or memory

   .. grid-item-card:: :octicon:`tools` Automation
      :link: docs-concepts-build-system
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Easier automated building, testing, and linting for Bazel, GN,
      and CMake projects

.. grid:: 2

   .. grid-item-card:: :octicon:`container` Environments
      :link: docs-concepts-development-environment
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Hermetic and replicable development environments for your entire team -
      no more struggling with toolchains and "it worked on my machine"

   .. grid-item-card:: :octicon:`stack` Frameworks
      :link: docs-concepts-full-framework
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Turnkey solutions for new projects that want to make full use of
      everything that Pigweed offers

--------------------
Who's using Pigweed?
--------------------
Pigweed has shipped on millions of devices.

--------------------
Showcase: pw_console
--------------------
:ref:`module-pw_console` is our multi-purpose, pluggable REPL and log viewer.
It's designed to be a complete development and manufacturing solution for
interacting with hardware devices via :ref:`module-pw_rpc` over a
:ref:`module-pw_hdlc` transport. Gone are the days of hacking together a REPL
and log viewer for each new project!

.. figure:: pw_console/images/python_completion.png
   :alt: Using pw_console to interact with a device

   Using pw_console to interact with a device

.. _docs-root-changelog:

---------------------
What's new in Pigweed
---------------------
.. include:: changelog.rst
   :start-after: .. changelog_highlights_start
   :end-before: .. changelog_highlights_end

See :ref:`docs-changelog-latest` in our changelog for details.

----------
Talk to us
----------
.. raw:: html

   <!-- Add a little space between the H2 and the cards. This is an edge case
        that only occurs on the homepage so it's best to just hack the fix
        locally here. -->
   <br>

.. grid:: 1

   .. grid-item-card:: :octicon:`comment-discussion` Chat room
      :link: https://discord.gg/M9NSeTA
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      For real-time discussion with the Pigweed team, head over to our Discord.

.. grid:: 2

   .. grid-item-card:: :octicon:`device-camera-video` Monthly community meeting
      :link: https://discord.com/channels/691686718377558037/951228399119126548
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      .. pigweed-live::

   .. grid-item-card:: :octicon:`bug` Issues
      :link: https://pwbug.dev
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      Found a bug? Got a feature request? Please create a new issue in our
      tracker.

.. toctree::
   :maxdepth: 1
   :hidden:

   Home <self>
   docs/overview
   docs/get_started/index
   docs/concepts/index
   targets
   modules
   changelog
   Mailing List <https://groups.google.com/forum/#!forum/pigweed>
   Chat Room <https://discord.gg/M9NSeTA>
   docs/os/index
   docs/size_optimizations
   Code Editor Support <docs/editors>
   third_party_support
   Source Code <https://cs.pigweed.dev/pigweed>
   Code Reviews <https://pigweed-review.googlesource.com>
   Issue Tracker <https://issues.pigweed.dev/issues?q=status:open>
   docs/contributing/index
   docs/infra/index
   Automated Analysis <automated_analysis>
   Build System <build_system>
   SEEDs <seed/0000-index>
   kudzu/docs
   Eng Blog <docs/blog/index>
