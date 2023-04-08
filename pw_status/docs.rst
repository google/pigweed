.. _module-pw_status:

=========
pw_status
=========
``pw_status`` provides features for communicating the result of an operation.
The classes in ``pw_status`` are used extensively throughout Pigweed.

------
Status
------
The primary feature of ``pw_status`` is the ``Status`` class (in C++) or enum
(in other languages). ``pw_status`` provides an implementation of status in
every supported Pigweed language.

The C++ implementation is :cpp:class:`pw::Status`, a simple, zero-overhead
status object that wraps a status code. Other languages use an enum.

Pigweed's status uses Google's standard status codes (see the `Google APIs
repository <https://github.com/googleapis/googleapis/blob/HEAD/google/rpc/code.proto>`_).
These codes are used extensively in Google projects including `Abseil
<https://abseil.io>`_ (`status/status.h
<https://cs.opensource.google/abseil/abseil-cpp/+/HEAD:absl/status/status.h>`_)
and `gRPC <https://grpc.io>`_ (`doc/statuscodes.md
<https://github.com/grpc/grpc/blob/HEAD/doc/statuscodes.md>`_).

Status codes
============
.. c:enumerator:: OK = 0

   ``OK`` does not indicate an error; this value is returned on success. It is
   typical to check for this value before proceeding on any given call across an
   API or RPC boundary. To check this value, use the ``ok()`` member function
   rather than inspecting the raw code.

   .. list-table::

      * - C++
        - ``pw::OkStatus()``
      * - C
        - ``PW_STATUS_OK``
      * - Python / Java / TypeScript
        - ``Status.OK``

.. c:enumerator:: CANCELLED = 1

   ``CANCELLED`` indicates the operation was cancelled, typically by the caller.

   .. list-table::

      * - C++
        - ``pw::Status::Cancelled()``
      * - C
        - ``PW_STATUS_CANCELLED``
      * - Python / Java / TypeScript
        - ``Status.CANCELLED``

.. c:enumerator:: UNKNOWN = 2

   ``UNKNOWN`` indicates an unknown error occurred. In general, more specific
   errors should be raised, if possible. Errors raised by APIs that do not
   return enough error information may be converted to this error.

   .. list-table::

      * - C++
        - ``pw::Status::Unknown()``
      * - C
        - ``PW_STATUS_UNKNOWN``
      * - Python / Java / TypeScript
        - ``Status.UNKNOWN``

.. c:enumerator:: INVALID_ARGUMENT = 3

   ``INVALID_ARGUMENT`` indicates the caller specified an invalid argument, such
   as a malformed filename. Note that use of such errors should be narrowly
   limited to indicate the invalid nature of the arguments themselves. Errors
   with validly formed arguments that may cause errors with the state of the
   receiving system should be denoted with :c:enumerator:`FAILED_PRECONDITION`
   instead.

   .. list-table::

      * - C++
        - ``pw::Status::InvalidArgument()``
      * - C
        - ``PW_STATUS_INVALID_ARGUMENT``
      * - Python / Java / TypeScript
        - ``Status.INVALID_ARGUMENT``

.. c:enumerator:: DEADLINE_EXCEEDED = 4

   ``DEADLINE_EXCEEDED`` indicates a deadline expired before the operation could
   complete. For operations that may change state within a system, this error
   may be returned even if the operation has completed successfully. For
   example, a successful response from a server could have been delayed long
   enough for the deadline to expire.

   .. list-table::

      * - C++
        - ``pw::Status::DeadlineExceeded()``
      * - C
        - ``PW_STATUS_DEADLINE_EXCEEDED``
      * - Python / Java / TypeScript
        - ``Status.DEADLINE_EXCEEDED``

.. c:enumerator:: NOT_FOUND = 5

   ``NOT_FOUND`` indicates some requested entity (such as a file or directory)
   was not found.

   :c:enumerator:`NOT_FOUND` is useful if a request should be denied for an
   entire class of users, such as during a gradual feature rollout or
   undocumented allowlist. If a request should be denied for specific sets of
   users, such as through user-based access control, use
   :c:enumerator:`PERMISSION_DENIED` instead.

   .. list-table::

      * - C++
        - ``pw::Status::NotFound()``
      * - C
        - ``PW_STATUS_NOT_FOUND``
      * - Python / Java / TypeScript
        - ``Status.NOT_FOUND``

.. c:enumerator:: ALREADY_EXISTS = 6

   ``ALREADY_EXISTS`` indicates that the entity a caller attempted to create
   (such as a file or directory) is already present.

   .. list-table::

      * - C++
        - ``pw::Status::AlreadyExists()``
      * - C
        - ``PW_STATUS_ALREADY_EXISTS``
      * - Python / Java / TypeScript
        - ``Status.ALREADY_EXISTS``

.. c:enumerator:: PERMISSION_DENIED = 7

   ``PERMISSION_DENIED`` indicates that the caller does not have permission to
   execute the specified operation. Note that this error is different than an
   error due to an unauthenticated user. This error code does not imply the
   request is valid or the requested entity exists or satisfies any other
   pre-conditions.

   :c:enumerator:`PERMISSION_DENIED` must not be used for rejections caused by
   exhausting some resource. Instead, use :c:enumerator:`RESOURCE_EXHAUSTED` for
   those errors.  :c:enumerator:`PERMISSION_DENIED` must not be used if the
   caller cannot be identified.  Instead, use :c:enumerator:`UNAUTHENTICATED`
   for those errors.

   .. list-table::

      * - C++
        - ``pw::Status::PermissionDenied()``
      * - C
        - ``PW_STATUS_PERMISSION_DENIED``
      * - Python / Java / TypeScript
        - ``Status.PERMISSION_DENIED``

.. c:enumerator:: RESOURCE_EXHAUSTED = 8

   ``RESOURCE_EXHAUSTED`` indicates some resource has been exhausted, perhaps a
   per-user quota, or perhaps the entire file system is out of space.

   .. list-table::

      * - C++
        - ``pw::Status::ResourceExhausted()``
      * - C
        - ``PW_STATUS_RESOURCE_EXHAUSTED``
      * - Python / Java / TypeScript
        - ``Status.RESOURCE_EXHAUSTED``

.. c:enumerator:: FAILED_PRECONDITION = 9

   ``FAILED_PRECONDITION`` indicates that the operation was rejected because the
   system is not in a state required for the operation's execution. For example,
   a directory to be deleted may be non-empty, an ``rmdir`` operation is applied
   to a non-directory, etc.

   .. _module-pw_status-guidelines:

   Some guidelines that may help a service implementer in deciding between
   :c:enumerator:`FAILED_PRECONDITION`, :c:enumerator:`ABORTED`, and
   :c:enumerator:`UNAVAILABLE`:

   a. Use :c:enumerator:`UNAVAILABLE` if the client can retry just the failing
      call.
   b. Use :c:enumerator:`ABORTED` if the client should retry at a higher
      transaction level (such as when a client-specified test-and-set fails,
      indicating the client should restart a read-modify-write sequence).
   c. Use :c:enumerator:`FAILED_PRECONDITION` if the client should not retry
      until the system state has been explicitly fixed. For example, if a
      ``rmdir`` fails because the directory is non-empty,
      :c:enumerator:`FAILED_PRECONDITION` should be returned since the client
      should not retry unless the files are deleted from the directory.

   .. list-table::

      * - C++
        - ``pw::Status::FailedPrecondition()``
      * - C
        - ``PW_STATUS_FAILED_PRECONDITION``
      * - Python / Java / TypeScript
        - ``Status.FAILED_PRECONDITION``

.. c:enumerator:: ABORTED = 10

   ``ABORTED`` indicates the operation was aborted, typically due to a
   concurrency issue such as a sequencer check failure or a failed transaction.

   See the :ref:`guidelines <module-pw_status-guidelines>` above for deciding
   between :c:enumerator:`FAILED_PRECONDITION`, :c:enumerator:`ABORTED`, and
   :c:enumerator:`UNAVAILABLE`.

   .. list-table::

      * - C++
        - ``pw::Status::Aborted()``
      * - C
        - ``PW_STATUS_ABORTED``
      * - Python / Java / TypeScript
        - ``Status.ABORTED``

.. c:enumerator:: OUT_OF_RANGE = 11

   ``OUT_OF_RANGE`` indicates the operation was attempted past the valid range,
   such as seeking or reading past an end-of-file.

   Unlike :c:enumerator:`INVALID_ARGUMENT`, this error indicates a problem that
   may be fixed if the system state changes. For example, a 32-bit file system
   will generate :c:enumerator:`INVALID_ARGUMENT` if asked to read at an offset
   that is not in the range [0,2^32-1], but it will generate
   :c:enumerator:`OUT_OF_RANGE` if asked to read from an offset past the current
   file size.

   There is a fair bit of overlap between :c:enumerator:`FAILED_PRECONDITION`
   and :c:enumerator:`OUT_OF_RANGE`.  We recommend using
   :c:enumerator:`OUT_OF_RANGE` (the more specific error) when it applies so
   that callers who are iterating through a space can easily look for an
   :c:enumerator:`OUT_OF_RANGE` error to detect when they are done.

   .. list-table::

      * - C++
        - ``pw::Status::OutOfRange()``
      * - C
        - ``PW_STATUS_OUT_OF_RANGE``
      * - Python / Java / TypeScript
        - ``Status.OUT_OF_RANGE``

.. c:enumerator:: UNIMPLEMENTED = 12

   ``UNIMPLEMENTED`` indicates the operation is not implemented or supported in
   this service. In this case, the operation should not be re-attempted.

   .. list-table::

      * - C++
        - ``pw::Status::Unimplemented()``
      * - C
        - ``PW_STATUS_UNIMPLEMENTED``
      * - Python / Java / TypeScript
        - ``Status.UNIMPLEMENTED``

.. c:enumerator:: INTERNAL = 13

   ``INTERNAL`` indicates an internal error has occurred and some invariants
   expected by the underlying system have not been satisfied. This error code is
   reserved for serious errors.

   .. list-table::

      * - C++
        - ``pw::Status::Internal()``
      * - C
        - ``PW_STATUS_INTERNAL``
      * - Python / Java / TypeScript
        - ``Status.INTERNAL``

.. c:enumerator:: UNAVAILABLE = 14

   ``UNAVAILABLE`` indicates the service is currently unavailable and that this
   is most likely a transient condition. An error such as this can be corrected
   by retrying with a backoff scheme. Note that it is not always safe to retry
   non-idempotent operations.

   See the :ref:`guidelines <module-pw_status-guidelines>` above for deciding
   between :c:enumerator:`FAILED_PRECONDITION`, :c:enumerator:`ABORTED`, and
   :c:enumerator:`UNAVAILABLE`.

   .. list-table::

      * - C++
        - ``pw::Status::Unavailable()``
      * - C
        - ``PW_STATUS_UNAVAILABLE``
      * - Python / Java / TypeScript
        - ``Status.UNAVAILABLE``

.. c:enumerator:: DATA_LOSS = 15

   ``DATA_LOSS`` indicates that unrecoverable data loss or corruption has
   occurred. As this error is serious, proper alerting should be attached to
   errors such as this.

   .. list-table::

      * - C++
        - ``pw::Status::DataLoss()``
      * - C
        - ``PW_STATUS_DATA_LOSS``
      * - Python / Java / TypeScript
        - ``Status.DATA_LOSS``

.. c:enumerator:: UNAUTHENTICATED = 16

   ``UNAUTHENTICATED`` indicates that the request does not have valid
   authentication credentials for the operation. Correct the authentication and
   try again.

   .. list-table::

      * - C++
        - ``pw::Status::Unauthenticated()``
      * - C
        - ``PW_STATUS_UNAUTHENTICATED``
      * - Python / Java / TypeScript
        - ``Status.UNAUTHENTICATED``

C++ API
=======
.. doxygenclass:: pw::Status
   :members:

.. doxygenfunction:: pw::OkStatus

.. c:enum:: pw_Status

   Enum to use in place of :cpp:class:`pw::Status` in C code. Always use
   :cpp:class:`pw::Status` in C++ code.

   The values of the :c:enum:`pw_Status` enum are all-caps and prefixed with
   ``PW_STATUS_``. For example, ``PW_STATUS_DATA_LOSS`` corresponds with
   :c:enumerator:`DATA_LOSS`.

Tracking the first error encountered
------------------------------------
In some contexts it is useful to track the first error encountered while
allowing execution to continue. Manually writing out ``if`` statements to check
and then assign quickly becomes verbose, and doesn't explicitly highlight the
intended behavior of "latching" to the first error.

.. code-block:: cpp

  Status overall_status;
  for (Sector& sector : sectors) {
    Status erase_status = sector.Erase();
    if (!overall_status.ok()) {
      overall_status = erase_status;
    }

    if (erase_status.ok()) {
      Status header_write_status = sector.WriteHeader();
      if (!overall_status.ok()) {
        overall_status = header_write_status;
      }
    }
  }
  return overall_status;

:cpp:class:`pw::Status` has a :cpp:func:`pw::Status::Update()` helper function
that does exactly this to reduce visual clutter and succinctly highlight the
intended behavior.

.. code-block:: cpp

  Status overall_status;
  for (Sector& sector : sectors) {
    Status erase_status = sector.Erase();
    overall_status.Update(erase_status);

    if (erase_status.ok()) {
      overall_status.Update(sector.WriteHeader());
    }
  }
  return overall_status;

Unused result warnings
----------------------
If the ``PW_STATUS_CFG_CHECK_IF_USED`` option is enabled, ``pw::Status`` objects
returned from function calls must be used or it is a compilation error. To
silence these warnings call ``IgnoreError()`` on the returned status object.

``PW_STATUS_CFG_CHECK_IF_USED`` defaults to off. Pigweed compiles with this
option enabled, but projects that use Pigweed will need to be updated to compile
with this option. After all projects have migrated, unused result warnings will
be enabled unconditionally.

C compatibility
---------------
``pw_status`` provides the C-compatible :c:enum:`pw_Status` enum for the status
codes.  For ease of use, :cpp:class:`pw::Status` implicitly converts to and from
:c:enum:`pw_Status`.  However, the :c:enum:`pw_Status` enum should never be used
in C++; instead use the :cpp:class:`pw::Status` class.

--------------
StatusWithSize
--------------
``pw::StatusWithSize`` (``pw_status/status_with_size.h``) is a convenient,
efficient class for reporting a status along with an unsigned integer value.
It is similar to the ``pw::Result<T>`` class, but it stores both a size and a
status, regardless of the status value, and only supports a limited range (27
bits).

``pw::StatusWithSize`` values may be created with functions similar to
``pw::Status``. For example,

  .. code-block:: cpp

    // An OK StatusWithSize with a size of 123.
    StatusWithSize(123)

    // A NOT_FOUND StatusWithSize with a size of 0.
    StatusWithSize::NotFound()

    // A RESOURCE_EXHAUSTED StatusWithSize with a size of 10.
    StatusWithSize::ResourceExhausted(10)

------
PW_TRY
------
``PW_TRY`` (``pw_status/try.h``) is a convenient set of macros for working
with Status and ``StatusWithSize`` objects in functions that return Status or
``StatusWithSize``. The ``PW_TRY`` and ``PW_TRY_WITH_SIZE`` macros call a
function and do an early return if the function's return status is not ok.

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

``PW_TRY_ASSIGN`` is for working with ``StatusWithSize`` objects in in functions
that return Status. It is similar to ``PW_TRY`` with the addition of assigning
the size from the ``StatusWithSize`` on ok.

.. code-block:: cpp

  Status PwTryAssignExample() {
    size_t size_value
    PW_TRY_ASSIGN(size_value, FunctionThatReturnsStatusWithSize());

    // Do something that uses size_value. size_value is only assigned and this
    // following code executed if the PW_TRY_ASSIGN function above returns OK.
  }

Zephyr
======
To enable ``pw_status`` for Zephyr add ``CONFIG_PIGWEED_STATUS=y`` to the
project's configuration.
