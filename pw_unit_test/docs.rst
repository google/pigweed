.. default-domain:: cpp

.. highlight:: sh

.. _chapter-pw-unit-test:

------------
pw_unit_test
------------
``pw_unit_test`` unit testing library with a `Google Test`_-compatible API,
built on top of embedded-friendly primitives.

``pw_unit_test`` is a portable library which can run on almost any system from
from bare metal to a full-fledged desktop OS. It does this by offloading the
responsibility of test reporting and output to the underlying system,
communicating its results through a common interface. Unit tests can be written
once and run under many different environments, empowering developers to write
robust, high quality code.

``pw_unit_test`` is still under development and lacks many features expected in
a complete testing framework; nevertheless, it is already used heavily within
Pigweed.

.. note::

  This documentation is currently incomplete.

Writing unit tests
==================

``pw_unit_test``'s interface is largely compatible with `Google Test`_. Refer to
the Google Test documentation for examples of to define unit test cases.

.. note::

  A lot of Google Test's more advanced features are not yet implemented. To
  request a feature addition, please
  `file a bug <https://bugs.chromium.org/p/pigweed/issues/entry>`_.

Using the test framework
========================

The EventHandler interface
^^^^^^^^^^^^^^^^^^^^^^^^^^
The ``EventHandler`` class in ``public/pw_unit_test/event_handler.h`` defines
the interface through which ``pw_unit_test`` communicates the results of its
test runs. A platform using ``pw_unit_test`` must register an event handler with
the unit testing framework to receive test output.

As the framework runs tests, it calls the event handler's callback functions to
notify the system of various test events. The system can then choose to perform
any necessary handling or aggregation of these events, and report them back to
the developer.

Predefined event handlers
-------------------------
Pigweed provides some standard event handlers upstream to simplify the process
of getting started using ``pw_unit_test``.

* ``SimplePrintingEventHandler``: An event handler that writes Google Test-style
  output to a specified sink.

  .. code::

    [==========] Running all tests.
    [ RUN      ] Status.Default
    [       OK ] Status.Default
    [ RUN      ] Status.ConstructWithStatusCode
    [       OK ] Status.ConstructWithStatusCode
    [ RUN      ] Status.AssignFromStatusCode
    [       OK ] Status.AssignFromStatusCode
    [ RUN      ] Status.CompareToStatusCode
    [       OK ] Status.CompareToStatusCode
    [ RUN      ] Status.Ok_OkIsTrue
    [       OK ] Status.Ok_OkIsTrue
    [ RUN      ] Status.NotOk_OkIsFalse
    [       OK ] Status.NotOk_OkIsFalse
    [ RUN      ] Status.KnownString
    [       OK ] Status.KnownString
    [ RUN      ] Status.UnknownString
    [       OK ] Status.UnknownString
    [==========] Done running all tests.
    [  PASSED  ] 8 test(s).


* ``LoggingEventHandler``: An event handler which uses the ``pw_log`` module to
  output test results, to integrate with the system's existing logging setup.

.. _running-tests:

Running tests
^^^^^^^^^^^^^
To run unit tests, link the tests into a single binary with the unit testing
framework, register an event handler, and call the ``RUN_ALL_TESTS`` macro.

.. code:: cpp

  #include "pw_unit_test/framework.h"
  #include "pw_unit_test/simple_printing_event_handler.h"

  void WriteString(const std::string_view& string, bool newline) {
    printf("%s", string.data());
    if (newline) {
      printf("\n");
    }
  }

  int main() {
    pw::unit_test::SimplePrintingEventHandler handler(WriteString);
    pw::unit_test::RegisterEventHandler(&handler);
    return RUN_ALL_TESTS();
  }

Build system integration
^^^^^^^^^^^^^^^^^^^^^^^^
``pw_unit_test`` integrates directly into Pigweed's GN build system. To define
simple unit tests, set the ``pw_unit_test_main`` build variable to a target
which configures the test framework as described in the :ref:`running-tests`
section, and use the ``pw_test`` template to register your test code.

.. code::

  import("$dir_pw_unit_test/test.gni")

  pw_test("foo_test") {
    sources = [ "foo_test.cc" ]
  }

.. _Google Test: https://github.com/google/googletest/blob/master/googletest/docs/primer.md
