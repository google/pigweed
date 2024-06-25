.. _module-pw_hex_dump-api:

=============
API reference
=============
.. pigweed-module-subpage::
   :name: pw_hex_dump

------------
Core Classes
------------

.. _module-pw_hex_dump-api-formattedhexdumper:

FormattedHexDumper
==================

.. doxygenclass:: pw::dump::FormattedHexDumper
   :members:

-----------------
Utility Functions
-----------------

.. _module-pw_hex_dump-api-dumpaddr:

DumpAddr
========

.. doxygenfunction:: pw::dump::DumpAddr(span<char> dest, const void *ptr)
.. doxygenfunction:: pw::dump::DumpAddr(span<char> dest, uintptr_t addr)

.. _module-pw_hex_dump-api-logbytes:

LogBytes
========

.. doxygenfunction:: pw::dump::LogBytes
