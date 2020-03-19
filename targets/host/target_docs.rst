.. _chapter-host:

.. default-domain:: cpp

.. highlight:: sh

----
host
----
The Pigweed host target assembles Pigweed's reStructuredText and markdown
documentation into a collection of HTML pages.

Building
========
To build for this target, change the ``pw_target_config`` GN build arg to point
to this target's configuration file.

.. code:: sh

  $ gn gen --args='pw_target_config = "//targets/host/target_config.gni"' out/host
  $ ninja -C out/host

or

.. code:: sh

  $ gn gen out/host
  $ gn args
  # Modify and save the args file to update the pw_target_config.
  pw_target_config = "//targets/host/target_config.gni"
  $ ninja -C out/host


Running Tests
=============
Tests are automatically run as part of the host build, but if you desire to
manually run tests, you may invoke them from a shell directly.

Example:

... code:: sh

  $ ./out/host/obj/pw_status/status_test

Configuration
=============

The host target exposes a few options that may be used to change the host build
behavior.

pw_target_toolchain
-------------------
The toolchain to build Pigweed with may be overriden using this build argument.
The default toolchain is ``pw_toolchain``'s ``host_gcc_og`` (Linux/Windows) or
``host_clang_og`` (macOS).

pw_build_host_tools
-------------------
Pigweed includes a number of host-only tooling that may be built as part of the
host build. These tools are included as part of the bootstrap, so it's only
necessary to enable this setting when modifying host tooling. This is
disabled by default.
