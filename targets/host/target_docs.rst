.. _target-host:

----
host
----
The Pigweed host target is used for unit testing and some host side tooling.

Building
========
To build for this target, invoke ninja with the top-level "host" group as the
target to build.

.. code:: sh

  $ ninja -C out host

There are two host toolchains, and both of them can be manually invoked by
replacing `host` with `host_clang` or `host_gcc`. Not all toolchains are
supported on all platforms. Unless working specifically on one toolchain, it is
recommended to leave this to the default.

Running Tests
=============
Tests are automatically run as part of the host build, but if you desire to
manually run tests, you may invoke them from a shell directly.

Example:

... code:: sh

  $ ./out/host_[compiler]_debug/obj/pw_status/status_test

Configuration
=============
The host target exposes a few options that may be used to change the host build
behavior.

pw_build_HOST_TOOLS
-------------------
Pigweed includes a number of host-only tooling that may be built as part of the
host build. These tools are included as part of the bootstrap, so it's only
necessary to enable this setting when modifying host tooling. This is
disabled by default.
