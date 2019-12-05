.. _chapter-pw-status:

.. default-domain:: cpp

.. highlight:: sh

---------
pw_status
---------
``pw::Status`` (``pw_status/status.h``) is a simple, zero-overhead status
object. It uses Google's standard status codes, which are also used by projects
such as `gRPC <https://github.com/grpc/grpc/blob/master/doc/statuscodes.md>`_.

``pw::StatusWithSize`` (``pw_status/status_with_size.h``) is a convenient,
efficent class for reporting a status along with an unsigned integer value.

The classes in pw_status are used extensively by other Pigweed modules.

Compatibility
=============
C++11
