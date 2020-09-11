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

``PW_TRY`` (``pw_status/try.h``) is a convenient set of macros for working
with Status and StatusWithSize objects in functions that return Status or
StatusWithSize. The PW_TRY and PW_TRY_WITH_SIZE macros call a function and
do an early return if the function's return status is not ok.

Example:

.. code-block:: cpp

  Status PwTryExample() {
    PW_TRY(FunctionThatReturnsStatus());
    PW_TRY(FunctionThatReturnsStatusWithSize());

    // Do something, only executed if both functions above return OK.
  }

  StatusWithSize PwTryWithSizeExample() {
    PW_TRY_WITH_SIZE(FunctionThatReturnsStatus());
    PW_TRY_WITH_SIZE(FunctionThatReturnsStatusWithSize());

    // Do something, only executed if both functions above return OK.
  }

PW_TRY_ASSIGN is for working with StatusWithSize objects in in functions
that return Status. It is similar to PW_TRY with the addition of assigning
the size from the StatusWithSize on ok.

.. code-block:: cpp

  Status PwTryAssignExample() {
    size_t size_value
    PW_TRY_ASSIGN(size_value, FunctionThatReturnsStatusWithSize());

    // Do something that uses size_value. size_value is only assigned and this
    // following code executed if the PW_TRY_ASSIGN function above returns OK.
  }

The classes in pw_status are used extensively by other Pigweed modules.

Compatibility
=============
C++11
