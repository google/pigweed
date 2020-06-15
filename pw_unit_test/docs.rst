.. _chapter-pw-unit-test:
.. default-domain:: cpp

.. highlight:: sh

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
  `let us know <mailto:pigweed@googlegroups.com>`_.

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
simple unit tests, set the ``pw_unit_test_MAIN`` build variable to a target
which configures the test framework as described in the :ref:`running-tests`
section, and use the ``pw_test`` template to register your test code.

.. code::

  import("$dir_pw_unit_test/test.gni")

  pw_test("foo_test") {
    sources = [ "foo_test.cc" ]
  }

The ``pw_unit_test`` module provides a few optional libraries to simplify setup:

 - ``simple_printing_event_handler```: When running tests, output test results
   as plain text over ``pw_sys_io``.
 - ``simple_printing_main``: Implements a ``main()`` function that simply runs
   tests using the ``simple_printing_event_handler``.
 - ``logging_event_handler``: When running tests, log test results as
   plain text using pw_log (ensure your target has set a ``pw_log`` backend).
 - ``logging_main``: Implements a ``main()`` function that simply runs tests
   using the ``logging_event_handler``.


pw_test template
----------------

``pw_test`` defines a single test binary. It wraps ``pw_executable`` and pulls
in the test framework as well as the test entry point defined by the
``pw_unit_test_main`` build variable.

**Arguments**

* All GN executable arguments are accepted and forwarded to the underlying
  ``pw_executable``.
* ``enable_if``: Boolean indicating whether the test should be built. If false,
  replaces the test with an empty target. Default true.
* ``test_main``: Target label to add to the tests's dependencies to provide the
  ``main()`` function. Defaults to ``pw_unit_test_MAIN``. Set to ``""`` if
  ``main()`` is implemented in the test's ``sources``.

**Example**

.. code::

  import("$dir_pw_unit_test/test.gni")

  pw_test("large_test") {
    sources = [ "large_test.cc" ]
    enable_if = device_has_1m_flash
  }


pw_test_group template
----------------------

``pw_test_group`` defines a collection of tests or other test groups. Each
module should expose a ``pw_test_group`` called ``tests`` with the module's test
binaries.

**Arguments**

* ``tests``: List of the ``pw_test`` targets in the group.
* ``group_deps``: List of other ``pw_test_group`` targets on which this one
  depends.
* ``enable_if``: Boolean indicating whether the group target should be created.
  If false, an empty GN group is created instead. Default true.

**Example**

.. code::

  import("$dir_pw_unit_test/test.gni")

  pw_test_group("tests") {
    tests = [
      ":bar_test",
      ":foo_test",
    ]
  }

  pw_test("foo_test") {
    # ...
  }

  pw_test("bar_test") {
    # ...
  }


.. _Google Test: https://github.com/google/googletest/blob/master/googletest/docs/primer.md
