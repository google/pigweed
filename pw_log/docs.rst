.. _chapter-pw-log:

.. default-domain:: cpp

.. highlight:: cpp

------
pw_log
------
Pigweed's logging module provides facilities for applications to log
information about the execution of their application. The module is split into
two components:

1. The facade (this module) which is only a macro interface layer
2. The backend, provided elsewhere, that implements the low level logging

Basic usage example
-------------------

.. code-block:: cpp

  #define PW_LOG_MODULE_NAME "BLE"
  #include "pw_log/log.h"

  int main() {
    PW_LOG_INFO("Booting...");
    if (BootFailed()) {
      PW_LOG_CRITICAL("Had trouble booting due to error %d", GetErrorCode());
      ReportErrorsAndHalt();
    }
    PW_LOG_INFO("Successfully booted");
  }


Logging macros
--------------

.. cpp:function:: PW_LOG(level, flags, fmt, ...)

  This is the primary mechanism for logging.

  *level* - An integer level as defined by ``pw_log/levels.h``.

  *flags* - Arbitrary flags the backend can leverage. The semantics of these
  flags are not defined in the facade, but are instead meant as a general
  mechanism for communication bits of information to the logging backend.

  Here are some ideas for what a backend might use flags for:

  - Example: ``HAS_PII`` - A log has personally-identifying data
  - Example: ``HAS_DII`` - A log has device-identifying data
  - Example: ``RELIABLE_DELIVERY`` - Ask the backend to ensure the log is
    delivered; this may entail blocking other logs.
  - Example: ``BEST_EFFORT`` - Don't deliver this log if it would mean blocking
    or dropping important-flagged logs

  *fmt* - The message to log, which may contain format specifiers like ``%d``
  or ``%0.2f``.

  Example:

  .. code-block:: cpp

    PW_LOG(PW_NO_FLAGS, PW_LOG_LEVEL_INFO, "Temperature is %d degrees", temp);
    PW_LOG(UNRELIABLE_DELIVERY, PW_LOG_LEVEL_ERROR, "It didn't work!");

  .. note::

    ``PW_LOG()`` should not be used frequently; typically only when adding
    flags to a particular message to mark PII or to indicate delivery
    guarantees.  For most cases, prefer to use the direct ``PW_LOG_DEBUG``
    style macros, which are often implemented more efficiently in the backend.


.. cpp:function:: PW_LOG_DEBUG(fmt, ...)
.. cpp:function:: PW_LOG_INFO(fmt, ...)
.. cpp:function:: PW_LOG_WARN(fmt, ...)
.. cpp:function:: PW_LOG_ERROR(fmt, ...)
.. cpp:function:: PW_LOG_CRITICAL(fmt, ...)

  Shorthand for `PW_LOG(PW_LOG_NO_FLAGS, <level>, fmt, ...)`.

Logging attributes
------------------

The logging facade in Pigweed is designed to facilitate the capture of at least
the following attributes:

- *Level* - The log level; for example, INFO, DEBUG, ERROR, etc. Typically an
  integer
- *Flags* - Bitset for e.g. RELIABLE_DELIVERY, or HAS_PII, or BEST_EFFORT
- *File* - The file where the log was triggered
- *Line* - The line number in the file where the log line occured
- *Function* - What function the log comes from. This is expensive in binary
  size to use!
- *Module* - The user-defined module name for the log statement; e.g. “BLE” or
  “BAT”
- *Message* - The message itself; with % format arguments
- *Arguments* - The format arguments to message
- *Thread* - For devices running with an RTOS, capturing the thread is very
  useful
- *Others* - Processor security level? Maybe Thread is a good proxy for this

Each backend may decide to capture different attributes to balance the tradeoff
between call site code size, call site run time, wire format size, logging
complexity, and more.
