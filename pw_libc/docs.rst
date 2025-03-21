.. _module-pw_libc:

-------
pw_libc
-------
.. pigweed-module::
   :name: pw_libc

The ``pw_libc`` module provides a restricted subset of libc suitable for some
microcontroller projects.

Noop IO Backend
===============
llvm-libc expects platforms to provide a backend for stdio. The `noop_io`
backend is a trivial backend that does nothing but provides the required symbols
to link.
