.. _module-pw_log_string:

=============
pw_log_string
=============
``pw_log_string`` is a partial backend for ``pw_log``. This backend defines a
C API that the ``PW_LOG_*`` macros will call out to. ``pw_log_string`` does not
implement the C API, leaving projects to provide their own implementation.
See ``pw_log_basic`` for a similar ``pw_log`` backend that also provides an
implementation.

As this module passes the log message, file name, and module name as a string to
the handler function, it's relatively expensive and not well suited for
space-constrained devices. This module is oriented towards usage on a host
(e.g. a simulated device).

---------------
Getting started
---------------
This module is extremely minimal to set up:

1. Implement ``pw_log_string_HandleMessage()``
2. Set ``pw_log_BACKEND`` to ``"$dir_pw_log_string"``
3. Set ``pw_log_string_BACKEND`` to point to the source set that implements
   ``pw_log_string_HandleMessage()``

What exactly ``pw_log_string_HandleMessage()`` should do is entirely up to the
implementation. ``pw_log_basic``'s log handler is one example, but it's also
possible to encode as protobuf and send over a TCP port, write to a file, or
blink an LED to log as morse code.
