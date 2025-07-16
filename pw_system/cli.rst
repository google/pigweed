.. _module-pw_system-cli:

=====================
Console CLI reference
=====================
These are the standard pw_system console command line arguments.

.. argparse::
   :module: pw_system.console
   :func: get_parser
   :prog: pw console
   :nodefaultconst:
   :nodescription:
   :noepilog:

===================
Python Console APIs
===================
Functions for establishing a connection to pw_system powered devices using Python.

-------------------
Console startup API
-------------------
.. autofunction:: pw_system.console.main

---------------------
Device connection API
---------------------
Device connections can be established in the same way as the pw_system console
but through the Python function :py:func:`create_device_serial_or_socket_connection`.

.. autofunction:: pw_system.device_connection.create_device_serial_or_socket_connection
