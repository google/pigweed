.. _docs-get-started:

===========
Get Started
===========
.. _docs-first-time-setup:

--------------------------------------------------
Configure your workstation for Pigweed development
--------------------------------------------------
Pigweed does its best to bundle all its dependencies into an isolated
environment using :ref:`module-pw_env_setup`. While this doesn't eliminate
all prerequisites, it greatly accelerates new developer onboarding.

The first-time setup guides below are required for any Pigweed-based project,
but only need to be done once per machine.

.. grid:: 1

   .. grid-item-card:: First-time setup
      :link: docs-first-time-setup-guide
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Prerequisites, first-time setup, and support notes for Linux, macOS, and
      Windows.

------------------------------
Create a Pigweed-based project
------------------------------
.. grid:: 1

   .. grid-item-card:: :octicon:`rocket` Bazel
      :link: docs-get-started-bazel
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Fork our minimal, Bazel-based starter code. Bazel is the recommended
      build system for new projects using Pigweed.

.. grid:: 2

   .. grid-item-card:: :octicon:`code` Sample Project
      :link: https://pigweed.googlesource.com/pigweed/sample_project/+/main/README.md
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      Fork the Sample Project, a repository that outlines the recommended way to use
      Pigweed in a broader GN-based project. Note that Bazel is the recommended
      build system for new projects using Pigweed, whereas this sample project uses
      GN.

   .. grid-item-card:: :octicon:`code` Kudzu
      :link: docs-kudzu
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Study the code of Kudzu, a just-for-fun Maker Faire 2023 project that
      demonstrates complex Pigweed usage. This project also uses GN.

.. grid:: 2

   .. grid-item-card:: :octicon:`list-ordered` Zephyr
      :link: docs-os-zephyr-get-started
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Instructions on how to use Pigweed in a Zephyr-based project.

   .. grid-item-card:: :octicon:`list-ordered` Upstream Pigweed
      :link: docs-get-started-upstream
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Get set up to contribute to upstream Pigweed.

.. toctree::
   :maxdepth: 1
   :hidden:

   First-time setup <first_time_setup>
   Bazel quickstart <bazel>
   Bazel integration <bazel_integration>
   Upstream Pigweed <upstream>

------------------------------------------
Use Pigweed modules in an existing project
------------------------------------------
Pigweed is modular: you can use as much or as little of it as you need.

.. grid:: 1

   .. grid-item-card:: :octicon:`rocket` Bazel
      :link: docs-bazel-integration
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Instructions for how to use a Pigweed module in an existing Bazel
      project.
