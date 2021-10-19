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

RPC server
==========
The host target implements a system RPC server that runs over a local socket,
defaulting to port 33000. To communicate with a process running the host RPC
server, use ``pw rpc -s localhost:33000 <protos>``.
