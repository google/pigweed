.. _module-pw_rust:

=======
pw_rust
=======
Rust support in pigweed is **highly** experimental.  Currently functionality
is split between Bazel and GN support.

-----
Bazel
-----
Bazel support is based on `rules_rust <https://github.com/bazelbuild/rules_rust>`_
and supports a rich set of targets for both host and target builds.

Building and Running the Embedded Example
=========================================
The ``embedded_hello`` example can be built for both the ``lm3s6965evb``
and ``microbit`` QEMU machines.  The example can be built and run using
the following commands where ``PLATFORM`` is one of ``lm3s6965evb`` or
``microbit``.

.. code-block:: bash

   $ bazel build //pw_rust/examples/embedded_hello:hello \
     --platforms //pw_build/platforms:${PLATFORM} \

   $ qemu-system-arm \
     -machine ${PLATFORM} \
     -nographic \
     -semihosting-config enable=on,target=native \
     -kernel ./bazel-bin/pw_rust/examples/embedded_hello/hello
   Hello, Pigweed!

--
GN
--
In GN, currently only building a single host binary using the standard
libraries is supported.  Windows builds are currently unsupported.

Building
========
To build the sample rust targets, you need to enable
``pw_rust_ENABLE_EXPERIMENTAL_BUILD``:

.. code-block:: bash

   $ gn gen out --args="pw_rust_ENABLE_EXPERIMENTAL_BUILD=true"

Once that is set, you can build and run the ``hello`` example:

.. code-block:: bash

   $ ninja -C out host_clang_debug/obj/pw_rust/example/bin/hello
   $ ./out/host_clang_debug/obj/pw_rust/examples/host_executable/bin/hello
   Hello, Pigweed!

------------------
Third Party Crates
------------------
Thrid party crates are vendored in the
`third_party/rust_crates <https://pigweed.googlesource.com/third_party/rust_crates>`_
respository.  Currently referencing these is only supported through the bazel
build.
