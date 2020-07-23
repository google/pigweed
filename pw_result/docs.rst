.. default-domain:: cpp

.. highlight:: sh

.. _chapter-pw-result:

---------
pw_result
---------
``pw::Result`` is a convenient wrapper around returning a Status along side some
data when the status is OK. This is meant for returning lightweight result
types or references to larger results.

.. warning::

  Be careful not to use larger types by value as this can quickly consume
  unnecessary stack.

.. warning::

  This module is experimental. Its impact on code size and stack usage has not
  yet been profiled. Use at your own risk.

Compatibility
=============
Works with C++11, but some features require C++17.
