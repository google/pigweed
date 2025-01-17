.. _module-pw_atomic:

=========
pw_atomic
=========
.. pigweed-module::
   :name: pw_atomic

``pw_atomic`` provides software implementations of atomic memory operations.

The module exposes build targets which implement the atomic function API
expected by ``stdatomic.h`` for specific architecture targets. These libraries
are not intended to be directly depended on by other modules, but instead linked
into the final application images compiled by the build.

Initially, support for Cortex M0 devices is provided.
