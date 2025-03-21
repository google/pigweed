.. _module-pw_libcxx:

---------
pw_libcxx
---------
.. pigweed-module::
   :name: pw_libcxx

The ``pw_libcxx`` module provides symbols that would normally be provided by the
`libc++abi` library, and will eventually facilitate pulling in headers as well.
Currently, a basic implementation is provided on top of llvm-libc.
