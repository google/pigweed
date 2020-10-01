.. _module-pw_log:

------
pw_log
------
Pigweed's logging module provides facilities for applications to log
information about the execution of their application. The module is split into
two components:

1. The facade (this module) which is only a macro interface layer
2. The backend, provided elsewhere, that implements the low level logging

Usage examples
--------------
Here is a typical usage example, showing setting the module name, and using the
long-form names.

.. code-block:: cpp

  #define PW_LOG_MODULE_NAME "BLE"

  #include "pw_log/log.h"

  int main() {
    PW_LOG_INFO("Booting...");
    PW_LOG_DEBUG("CPU temp: %.2f", cpu_temperature);
    if (BootFailed()) {
      PW_LOG_CRITICAL("Had trouble booting due to error %d", GetErrorCode());
      ReportErrorsAndHalt();
    }
    PW_LOG_INFO("Successfully booted");
  }

In ``.cc`` files, it is possible to dispense with the ``PW_`` part of the log
names and go for shorter log macros.

.. code-block:: cpp

  #define PW_LOG_MODULE_NAME "BLE"
  #define PW_LOG_USE_ULTRA_SHORT_NAMES 1

  #include "pw_log/log.h"

  int main() {
    INF("Booting...");
    DBG("CPU temp: %.2f", cpu_temperature);
    if (BootFailed()) {
      CRT("Had trouble booting due to error %d", GetErrorCode());
      ReportErrorsAndHalt();
    }
    INF("Successfully booted");
  }

Layer diagram example: ``stm32f429i-disc1``
-------------------------------------------
Below is an example diagram showing how the modules connect together for the
``stm32f429i-disc1`` target, where the ``pw_log`` backend is ``pw_log_basic``.
``pw_log_basic`` uses the ``pw_sys_io`` module to log in plaintext, which in
turn outputs to the STM32F429 bare metal backend for ``pw_sys_io``, which is
``pw_sys_io_baremetal_stm32f429i``.

.. blockdiag::

  blockdiag {
    default_fontsize = 14;
    orientation = portrait;

    group {
      color = "#AAAAAA";
      label = "Microcontroller"

      app       [label = "App code"];
      facade    [label = "pw_log"];
      backend   [label = "pw_log_basic"];
      sys_io    [label = "pw_sys_io"];
      sys_io_bm [label = "pw_sys_io_\nstm32f429"];
      uart      [label = "UART pins"];
    }
    ftdi     [label = "FTDI cable"];
    computer [label = "Minicom"];

    app -> facade -> backend -> sys_io -> sys_io_bm -> uart -> ftdi -> computer;

    //app -> facade [folded];
    //backend -> sys_io [folded];
    //uart -> ftdi [folded];
  }

Logging macros
--------------

These are the primary macros for logging information about the functioning of a
system, intended to be used directly.

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

    PW_LOG(PW_LOG_DEFAULT_FLAGS, PW_LOG_LEVEL_INFO, "Temp is %d degrees", temp);
    PW_LOG(UNRELIABLE_DELIVERY, PW_LOG_LEVEL_ERROR, "It didn't work!");

  .. note::

    ``PW_LOG()`` should not be used frequently; typically only when adding
    flags to a particular message to mark PII or to indicate delivery
    guarantees.  For most cases, prefer to use the direct ``PW_LOG_INFO`` or
    ``PW_LOG_DEBUG`` style macros, which are often implemented more efficiently
    in the backend.


.. cpp:function:: PW_LOG_DEBUG(fmt, ...)
.. cpp:function:: PW_LOG_INFO(fmt, ...)
.. cpp:function:: PW_LOG_WARN(fmt, ...)
.. cpp:function:: PW_LOG_ERROR(fmt, ...)
.. cpp:function:: PW_LOG_CRITICAL(fmt, ...)

  Shorthand for `PW_LOG(PW_LOG_DEFAULT_FLAGS, <level>, fmt, ...)`.

Option macros
-------------
This module defines macros that can be overridden to control the behavior of
``pw_log`` statements. To override these macros, add ``#define`` statements
for them before including headers.

The option macro definitions must be visibile to ``pw_log/log.h`` the first time
it is included. To handle potential transitive includes, place these
``#defines`` before all ``#include`` statements. This should only be done in
source files, not headers. For example:

  .. code-block:: cpp

    // Set the pw_log option macros here, before ALL of the #includes.
    #define PW_LOG_MODULE_NAME "Calibration"
    #define PW_LOG_LEVEL PW_LOG_LEVEL_WARN

    #include <array>
    #include <random>

    #include "devices/hal9000.h"
    #include "pw_log/log.h"
    #include "pw_rpc/server.h"

    int MyFunction() {
      PW_LOG_INFO("hello???");
    }

.. c:macro:: PW_LOG_MODULE_NAME

  A string literal module name to use in logs. Log backends may attach this
  name to log messages or use it for runtime filtering. Defaults to ``""``. The
  ``PW_LOG_MODULE_NAME_DEFINED`` macro is set to ``1`` or ``0`` to indicate
  whether ``PW_LOG_MODULE_NAME`` was overridden.

.. c:macro:: PW_LOG_DEFAULT_FLAGS

  Log flags to use for the ``PW_LOG_<level>`` macros. Different flags may be
  applied when using the ``PW_LOG`` macro directly.

  Log backends use flags to change how they handle individual log messages.
  Potential uses include assigning logs priority or marking them as containing
  personal information. Defaults to ``0``.

.. c:macro:: PW_LOG_LEVEL

   Filters logs by level. Source files that define ``PW_LOG_LEVEL`` will display
   only logs at or above the chosen level. Log statements below this level will
   be compiled out of optimized builds. Defaults to ``PW_LOG_LEVEL_DEBUG``.

   Example:

   .. code-block:: cpp

     #define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

     #include "pw_log/log.h"

     void DoSomething() {
       PW_LOG_DEBUG("This won't be logged at all");
       PW_LOG_INFO("This is INFO level, and will display");
       PW_LOG_WARN("This is above INFO level, and will display");
     }

.. c:function:: PW_LOG_ENABLE_IF(level, flags)

   Filters logs by an arbitrary expression based on ``level`` and ``flags``.
   Source files that define ``PW_LOG_ENABLE_IF(level, flags)`` will display if
   the given expression evaluates true.

   Example:

   .. code-block:: cpp

     // Pigweed's log facade will call this macro to decide to log or not. In
     // this case, it will drop logs with the PII flag set if display of PII is
     // not enabled for the application.
     #define PW_LOG_ENABLE_IF(level, flags) \
         (level >= PW_LOG_LEVEL_INFO && \
          !((flags & MY_PRODUCT_PII_MASK) && MY_PRODUCT_LOG_PII_ENABLED)

     #include "pw_log/log.h"

     // This define might be supplied by the build system.
     #define MY_PRODUCT_LOG_PII_ENABLED false

     // This is the PII mask bit selected by the application.
     #define MY_PRODUCT_PII_MASK (1 << 5)

     void DoSomethingWithSensitiveInfo() {
       PW_LOG_DEBUG("This won't be logged at all");
       PW_LOG_INFO("This is INFO level, and will display");

       // In this example, this will not be logged since logging with PII
       // is disabled by the above macros.
       PW_LOG(PW_LOG_LEVEL_INFO,
              MY_PRODUCT_PII_MASK,
              "Sensitive: %d",
              sensitive_info);
     }

.. attention::

  At this time, only compile time filtering is supported. In the future, we
  plan to add support for runtime filtering.

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

Design discussion
-----------------

Why not use C++ style stream logging operators like Google Log?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
There are multiple reasons to avoid the C++ stream logging style in embedded,
but the biggest reason is that C++ stream logging defeats log tokenization. By
having the string literals broken up between ``<<`` operators, tokenization
becomes impossible with current language features.

Consider this example use of Google Log:

.. code-block:: cpp

  LOG(INFO) << "My temperature is " << temperature << ". State: " << state;

This log statement has two string literals. It might seem like one could convert
move to tokenization:

.. code-block:: cpp

  LOG(INFO) << TOKEN("My temperature is ") << temperature << TOKEN(". State: ") << state;

However, this doesn't work. The key problem is that the tokenization system
needs to allocate the string in a linker section that is excluded from the
final binary, but is in the final ELF executable (and so can be extracted).
Since there is no way to declare a string or array in a different section in
the middle of an experession in C++, it is not possible to tokenize an
expression like the above.

In contrast, the ``printf``-style version is a single statement with a single
string constant, which can be expanded by the preprocessor (as part of
``pw_tokenizer``) into a constant array in a special section.

.. code-block:: cpp

  // Note: LOG_INFO can be tokenized behind the macro; transparent to users.
  PW_LOG_INFO("My temperature is %d. State: %s", temperature, state);

Additionally, while Pigweed is mostly C++, it a practical reality that at times
projects using Pigweed will need to log from third-party libraries written in
C. Thus, we also wanted to retain C compatibility.

In summary, printf-style logging is better for Pigweed's target audience
because it:

- works with tokenization
- is C compatibile
- has smaller call sites

The Pigweed authors additionally maintain a C++ stream-style embedded logging
library for compatibility with non-embedded code. While it is effective for
porting server code to microcontrollers quickly, we do not advise embedded
projects use that approach unless absolutely necessary.

- See also :ref:`module-pw_log_tokenized` for details on leveraging Pigweed's
  tokenizer module for logging.
- See also :ref:`module-pw_tokenizer` for details on Pigweed's tokenizer,
  which is useful for more than just logging.

Why does the facade use header redirection instead of C functions?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Without header redirection, it is not possible to do sophisticated macro
transforms in the backend. For example, to apply tokenization to log strings,
the backend must define the handling macros. Additionally, compile-time
filtering by log level or flags is not possible without header redirection.
While it may be possible to do the filtering in the facade, that would imply
having the same filtering implementation for all log handling, which is a
restriction we want to avoid.

Why is the module name done as a preprocessor define rather than an argument?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
APIs are a balance between power and ease of use. In the practical cases we
have seen over the years, most translation units only need to log to one
module, like ``"BLE"``, ``"PWR"``, ``"BAT"`` and so on. Thus, adding the
argument to each macro call seemed like too much. On the other hand, flags are
something that are typically added on a per-log-statement basis, and is why the
flags are added on a per-call basis (though hidden through the high-level
macros).
