.. _module-pw_libcxx:

---------
pw_libcxx
---------
The ``pw_libcxx`` module provides libcxx symbols, and will eventually facilitate
pulling in headers as well. Currently, none of the library is built from
upstream LLVM libcxx, instead the symbols provided should just crash in
an embedded context.
