.. _module-pw_status:

=========
pw_status
=========
.. pigweed-module::
   :name: pw_status
   :tagline: Exception-free error propagation for embedded
   :status: stable
   :languages: C++17, C, Python, Java, TypeScript, Rust

   - **Easy**: Simple to understand, includes convenient macro ``PW_TRY``
   - **Efficient**: No memory allocation, no exceptions
   - **Established**: Just like ``absl::Status``, deployed extensively at Google

   :cpp:type:`pw::Status` is Pigweed's error propagation primitive, enabling
   exception-free error handling. The primary feature of ``pw_status`` is the
   :cpp:class:`pw::Status` class, a simple, zero-overhead status object that
   wraps a status code, and the ``PW_TRY`` macro. For example:

   .. code-block:: cpp

      #include "pw_status/status.h"

      pw::Status ImuEnable() {
        if (!device_has_imu) {
          return Status::FailedPrecondition();
        }
        PW_TRY(ImuSpiSendEnable());  // Propagates failure on non-OK status.
        return pw::OkStatus();
      }

      void Initialize() {
        if (auto status = ImuEnable(); status.ok()) {
          PW_LOG_INFO("Imu initialized successfully")
        } else {
          if (status.IsFailedPrecondition()) {
            PW_LOG_WARNING("No IMU present");
          } else {
            PW_LOG_ERROR("Unknown error initializing IMU: %d", status.code());
          }
        }
      }

``pw_status`` provides an implementation of status in every supported
Pigweed language, including C, Rust, TypeScript, Java, and Python.

Pigweed's status matches Google's standard status codes (see the `Google APIs
repository
<https://github.com/googleapis/googleapis/blob/HEAD/google/rpc/code.proto>`_).
These codes are used extensively in Google projects including `Abseil
<https://abseil.io>`_ (`status/status.h
<https://cs.opensource.google/abseil/abseil-cpp/+/HEAD:absl/status/status.h>`_)
and `gRPC <https://grpc.io>`_ (`doc/statuscodes.md
<https://github.com/grpc/grpc/blob/HEAD/doc/statuscodes.md>`_).

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Get Started & Guides
      :link: module-pw_status-get-started
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Integrate pw_status into your project, see common uses

   .. grid-item-card:: :octicon:`code-square` API Reference
      :link: module-pw_status-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Detailed description of pw_status's methods.

---------------
Quick reference
---------------
See :ref:`module-pw_status-codes` for the precise semantics of each error, as
well as how to spell the status in each of our supported languages.  Click on
the status names below to jump directly to that error's reference.

.. list-table::
   :widths: 35 5 60
   :header-rows: 1

   * - Status
     - Code
     - Description
   * - :c:enumerator:`OK`
     - 0
     - Operation succeeded
   * - :c:enumerator:`CANCELLED`
     - 1
     - Operation was cancelled, typically by the caller
   * - :c:enumerator:`UNKNOWN`
     - 2
     - Unknown error occurred. Avoid this code when possible.
   * - :c:enumerator:`INVALID_ARGUMENT`
     - 3
     - Argument was malformed; e.g. invalid characters when parsing integer
   * - :c:enumerator:`DEADLINE_EXCEEDED`
     - 4
     - Deadline passed before operation completed
   * - :c:enumerator:`NOT_FOUND`
     - 5
     - The entity that the caller requested (e.g. file or directory) is not
       found
   * - :c:enumerator:`ALREADY_EXISTS`
     - 6
     - The entity that the caller requested to create is already present
   * - :c:enumerator:`PERMISSION_DENIED`
     - 7
     - Caller lacks permission to execute action
   * - :c:enumerator:`RESOURCE_EXHAUSTED`
     - 8
     - Insufficient resources to complete operation; e.g. supplied buffer is too
       small
   * - :c:enumerator:`FAILED_PRECONDITION`
     - 9
     - System isn't in the required state; e.g. deleting a non-empty directory
   * - :c:enumerator:`ABORTED`
     - 10
     - Operation aborted due to e.g. concurrency issue or failed transaction
   * - :c:enumerator:`OUT_OF_RANGE`
     - 11
     - Operation attempted out of range; e.g. seeking past end of file
   * - :c:enumerator:`UNIMPLEMENTED`
     - 12
     - Operation isn't implemented or supported
   * - :c:enumerator:`INTERNAL`
     - 13
     - Internal error occurred; e.g. system invariants were violated
   * - :c:enumerator:`UNAVAILABLE`
     - 14
     - Requested operation can't finish now, but may at a later time
   * - :c:enumerator:`DATA_LOSS`
     - 15
     - Unrecoverable data loss occurred while completing the requested operation
   * - :c:enumerator:`UNAUTHENTICATED`
     - 16
     - Caller does not have valid authentication credentials for the operation

.. toctree::
   :hidden:
   :maxdepth: 1

   guide
   reference
