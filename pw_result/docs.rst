.. _module-pw_result:

=========
pw_result
=========
``pw::Result<T>`` is a class template for use in returning either a
``pw::Status`` error or an object of type ``T``.

.. inclusive-language: disable

``pw::Result<T>``'s implementation is closely based on Abseil's `StatusOr<T>
class <https://github.com/abseil/abseil-cpp/blob/master/absl/status/statusor.h>`_.
There are a few differences:

.. inclusive-language: enable

* ``pw::Result<T>`` uses ``pw::Status``, which is much less sophisticated than
  ``absl::Status``.
* ``pw::Result<T>``'s functions are ``constexpr`` and ``pw::Result<T>`` may be
  used in ``constexpr`` statements if ``T`` is trivially destructible.

-----
Usage
-----
Usage of ``pw::Result<T>`` is identical to Abseil's ``absl::StatusOr<T>``.
See Abseil's `documentation
<https://abseil.io/docs/cpp/guides/status#returning-a-status-or-a-value>`_ and
`usage tips <https://abseil.io/tips/181>`_ for guidance.

``pw::Result<T>`` is returned from a function that may return ``pw::OkStatus()``
and a value or an error status and no value. If ``ok()`` is true, the
``pw::Result<T>`` contains a valid ``T``. Otherwise, it does not contain a ``T``
and attempting to access the value is an error.

``pw::Result<T>`` can be used to directly access the contained type:

.. code-block:: cpp

  #include "pw_result/result.h"

  if (pw::Result<Foo> foo = TryCreateFoo(); foo.ok()) {
    foo->DoBar();
  }

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

.. warning::

  Be careful not to use larger types by value as this can quickly consume
  unnecessary stack.

.. warning::

  This module is experimental. Its impact on code size and stack usage has not
  yet been profiled. Use at your own risk.

-----------
Size report
-----------
The table below showcases the difference in size between functions returning a
Status with an output pointer, and functions returning a Result, in various
situations.

Note that these are simplified examples which do not necessarily reflect the
usage of Result in real code. Make sure to always run your own size reports to
check if Result is suitable for you.

.. include:: result_size

------
Zephyr
------
To enable ``pw_result`` for Zephyr add ``CONFIG_PIGWEED_RESULT=y`` to the
project's configuration.
