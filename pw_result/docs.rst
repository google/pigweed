.. _module-pw_result:

---------
pw_result
---------
``pw::Result`` is a convenient wrapper around returning a Status along side some
data when the status is OK. This is meant for returning lightweight result
types or references to larger results.

``pw::Result`` is compatible with ``PW_TRY`` and ``PW_TRY_ASSIGN``, for example:

.. code-block:: cpp

  #include "pw_status/try.h"
  #include "pw_result/result.h"

  pw::Result<int> GetAnswer();  // Example function.

  pw::Status UseAnswer() {
    const pw::Result<int> answer = GetAnswer();
    if (!answer.ok()) {
      return answer.status();
    }
    if (answer.value() == 42) {
      WhatWasTheUltimateQuestion();
    }
    return pw::OkStatus();
  }

  pw::Status UseAnswerWithTry() {
    const pw::Result<int> answer = GetAnswer();
    PW_TRY(answer.status());
    if (answer.value() == 42) {
      WhatWasTheUltimateQuestion();
    }
    return pw::OkStatus();
  }

  pw::Status UseAnswerWithTryAssign() {
    PW_TRY_ASSIGN(const int answer, GetAnswer());
    if (answer == 42) {
      WhatWasTheUltimateQuestion();
    }
    return pw::OkStatus();
  }

``pw::Result`` can be used to directly access the contained type:

.. code-block:: cpp

  #include "pw_result/result.h"

  pw::Result<Foo> foo = TryCreateFoo();
  if (foo.ok()) {
    foo->DoBar();
  }

or in C++17:

.. code-block:: cpp

  if (pw::Result<Foo> foo = TryCreateFoo(); foo.ok()) {
    foo->DoBar();
  }

See `Abseil's StatusOr <https://abseil.io/tips/181>`_ for guidance on using a
similar type.

.. warning::

  Be careful not to use larger types by value as this can quickly consume
  unnecessary stack.

.. warning::

  This module is experimental. Its impact on code size and stack usage has not
  yet been profiled. Use at your own risk.

Compatibility
=============
Works with C++14, but some features require C++17.

Size report
===========
The table below showcases the difference in size between functions returning a
Status with an output pointer, and functions returning a Result, in various
situations.

Note that these are simplified examples which do not necessarily reflect the
usage of Result in real code. Make sure to always run your own size reports to
check if Result is suitable for you.

.. include:: result_size

Zephyr
======
To enable ``pw_result`` for Zephyr add ``CONFIG_PIGWEED_RESULT=y`` to the
project's configuration.
