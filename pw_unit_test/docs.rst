.. _module-pw_unit_test:

============
pw_unit_test
============
``pw_unit_test`` unit testing library with a `Google Test`_-compatible API,
built on top of embedded-friendly primitives.

.. _Google Test: https://github.com/google/googletest/blob/HEAD/docs/primer.md

``pw_unit_test`` is a portable library which can run on almost any system from
bare metal to a full-fledged desktop OS. It does this by offloading the
responsibility of test reporting and output to the underlying system,
communicating its results through a common interface. Unit tests can be written
once and run under many different environments, empowering developers to write
robust, high quality code.

``pw_unit_test`` is still under development and lacks many features expected in
a complete testing framework; nevertheless, it is already used heavily within
Pigweed.

.. note::

  This documentation is currently incomplete.

------------------
Writing unit tests
------------------
``pw_unit_test``'s interface is largely compatible with `Google Test`_. Refer to
the Google Test documentation for examples of to define unit test cases.

.. note::

  Many of Google Test's more advanced features are not yet implemented. Missing
  features include:

  * Any GoogleMock features (e.g. :c:macro:`EXPECT_THAT`)
  * Floating point comparison macros (e.g. :c:macro:`EXPECT_FLOAT_EQ`)
  * Death tests (e.g. :c:macro:`EXPECT_DEATH`); ``EXPECT_DEATH_IF_SUPPORTED``
    does nothing but silently passes
  * Value-parameterized tests

  To request a feature addition, please
  `let us know <mailto:pigweed@googlegroups.com>`_.

  See `Using upstream Googletest and Googlemock` below for information
  about using upstream Googletest instead.

------------------------
Using the test framework
------------------------

The EventHandler interface
==========================
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
=============
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

Test filtering
==============
If using C++17, filters can be set on the test framework to run only a subset of
the registered unit tests. This is useful when many tests are bundled into a
single application image.

Currently, only a test suite filter is supported. This is set by calling
``pw::unit_test::SetTestSuitesToRun`` with a list of suite names.

.. note::
  Test filtering is only supported in C++17.

Build system integration
========================
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

 - ``simple_printing_event_handler``: When running tests, output test results
   as plain text over ``pw_sys_io``.
 - ``simple_printing_main``: Implements a ``main()`` function that simply runs
   tests using the ``simple_printing_event_handler``.
 - ``logging_event_handler``: When running tests, log test results as
   plain text using pw_log (ensure your target has set a ``pw_log`` backend).
 - ``logging_main``: Implements a ``main()`` function that simply runs tests
   using the ``logging_event_handler``.


pw_test template
----------------
``pw_test`` defines a single unit test suite. It creates several sub-targets.

* ``<target_name>``: The test suite within a single binary. The test code is
  linked against the target set in the build arg ``pw_unit_test_MAIN``.
* ``<target_name>.run``: If ``pw_unit_test_AUTOMATIC_RUNNER`` is set, this
  target runs the test as part of the build.
* ``<target_name>.lib``: The test sources without ``pw_unit_test_MAIN``.

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
``pw_test_group`` defines a collection of tests or other test groups. It creates
several sub-targets:

* ``<target_name>``: The test group itself.
* ``<target_name>.run``: If ``pw_unit_test_AUTOMATIC_RUNNER`` is set, this
  target runs all of the tests in the group and all of its group dependencies
  individually.
* ``<target_name>.lib``: The sources of all of the tests in this group and its
  dependencies.
* ``<target_name>.bundle``: All of the tests in the group and its dependencies
  bundled into a single binary.
* ``<target_name>.bundle.run``: Automatic runner for the test bundle.

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

pw_facade_test template
-----------------------
Pigweed facade test templates allow individual unit tests to build under the
current device target configuration while overriding specific build arguments.
This allows these tests to replace a facade's backend for the purpose of testing
the facade layer.

.. warning::
   Facade tests are costly because each facade test will trigger a re-build of
   every dependency of the test. While this sounds excessive, it's the only
   technically correct way to handle this type of test.

.. warning::
   Some facade test configurations may not be compatible with your target. Be
   careful when running a facade test on a system that heavily depends on the
   facade being tested.

Build arguments
---------------

.. option:: pw_unit_test_AUTOMATIC_RUNNER <executable>

  Path to a test runner to automatically run unit tests after they are built.

  If set, a ``pw_test`` target's ``<target_name>.run`` action will invoke the
  test runner specified by this argument, passing the path to the unit test to
  run. If this is unset, the ``pw_test`` target's ``<target_name>.run`` step
  will do nothing.

  Targets that don't support parallelized execution of tests (e.g. a on-device
  test runner that must flash a device and run the test in serial) should
  set pw_unit_test_POOL_DEPTH to 1.

  Type: string (name of an executable on the PATH, or path to an executable)
  Usage: toolchain-controlled only

.. option:: pw_unit_test_AUTOMATIC_RUNNER_ARGS <args>

  An optional list of strings to pass as args to the test runner specified
  by pw_unit_test_AUTOMATIC_RUNNER.

  Type: list of strings (args to pass to pw_unit_test_AUTOMATIC_RUNNER)
  Usage: toolchain-controlled only

.. option:: pw_unit_test_AUTOMATIC_RUNNER_TIMEOUT <timeout_seconds>

  An optional timeout to apply when running the automatic runner. Timeout is
  in seconds. Defaults to empty which means no timeout.

  Type: string (number of seconds to wait before killing test runner)
  Usage: toolchain-controlled only

.. option:: pw_unit_test_PUBLIC_DEPS <dependencies>

  Additional dependencies required by all unit test targets. (For example, if
  using a different test library like Googletest.)

  Type: list of strings (list of dependencies as GN paths)
  Usage: toolchain-controlled only

.. option:: pw_unit_test_MAIN <source_set>

  Implementation of a main function for ``pw_test`` unit test binaries.

  Type: string (GN path to a source set)
  Usage: toolchain-controlled only

.. option:: pw_unit_test_POOL_DEPTH <pool_depth>

  The maximum number of unit tests that may be run concurrently for the
  current toolchain. Setting this to 0 disables usage of a pool, allowing
  unlimited parallelization.

  Note: A single target with two toolchain configurations (e.g. release/debug)
        will use two separate test runner pools by default. Set
        pw_unit_test_POOL_TOOLCHAIN to the same toolchain for both targets to
        merge the pools and force serialization.

  Type: integer
  Usage: toolchain-controlled only

.. option:: pw_unit_test_POOL_TOOLCHAIN <toolchain>

  The toolchain to use when referring to the pw_unit_test runner pool. When
  this is disabled, the current toolchain is used. This means that every
  toolchain will use its own pool definition. If two toolchains should share
  the same pool, this argument should be by one of the toolchains to the GN
  path of the other toolchain.

  Type: string (GN path to a toolchain)
  Usage: toolchain-controlled only

RPC service
===========
``pw_unit_test`` provides an RPC service which runs unit tests on demand and
streams the results back to the client. The service is defined in
``pw_unit_test_proto/unit_test.proto``, and implemented by the GN target
``$dir_pw_unit_test:rpc_service``.

To set up RPC-based unit tests in your application, instantiate a
``pw::unit_test::UnitTestService`` and register it with your RPC server.

.. code:: c++

  #include "pw_rpc/server.h"
  #include "pw_unit_test/unit_test_service.h"

  // Server setup; refer to pw_rpc docs for more information.
  pw::rpc::Channel channels[] = {
   pw::rpc::Channel::Create<1>(&my_output),
  };
  pw::rpc::Server server(channels);

  pw::unit_test::UnitTestService unit_test_service;

  void RegisterServices() {
    server.RegisterService(unit_test_services);
  }

All tests flashed to an attached device can be run via python by calling
``pw_unit_test.rpc.run_tests()`` with a RPC client services object that has
the unit testing RPC service enabled. By default, the results will output via
logging.

.. code:: python

  from pw_hdlc.rpc import HdlcRpcClient
  from pw_unit_test.rpc import run_tests

  PROTO = Path(os.environ['PW_ROOT'],
               'pw_unit_test/pw_unit_test_proto/unit_test.proto')

  client = HdlcRpcClient(serial.Serial(device, baud), PROTO)
  run_tests(client.rpcs())

pw_unit_test.rpc
----------------
.. automodule:: pw_unit_test.rpc
  :members: EventHandler, run_tests

Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration of
this module.

.. c:macro:: PW_UNIT_TEST_CONFIG_EVENT_BUFFER_SIZE

  The size of the event buffer that the UnitTestService contains.
  This buffer is used to encode events.  By default this is set to
  128 bytes.

.. c:macro:: PW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE

  The size of the memory pool to use for test fixture instances. By default this
  is set to 16K.

Using upstream Googletest and Googlemock
========================================

If you want to use the full upstream Googletest/Googlemock, you must do the
following:

* Set the GN var ``dir_pw_third_party_googletest`` to the location of the
  googletest source. You can use ``pw package install googletest`` to fetch the
  source if desired.
* Set the GN var ``pw_unit_test_MAIN = "//third_party/googletest:gmock_main"``
* Set the GN var ``pw_unit_test_PUBLIC_DEPS = [ "//third_party/googletest" ]``

.. note::

  Not all unit tests build properly with upstream Googletest yet. This is a
  work in progress.
