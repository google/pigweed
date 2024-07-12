.. _module-pw_system-cli:

===============================
pw_system console CLI reference
===============================
.. argparse::
   :module: pw_system.console
   :func: get_parser
   :prog: pw system console
   :nodefaultconst:
   :nodescription:
   :noepilog:

===============================
pw_system Device connection API
===============================
Device connections can be established in the same way as the pw_system console
but through the Python function :py:func:`create_device_serial_or_socket_connection`.

.. autofunction:: pw_system.device_connection.create_device_serial_or_socket_connection
