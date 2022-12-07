.. _module-pw_perf_test:

============
pw_perf_test
============
Pigweed's perf test module provides an easy way to measure performance on
any test setup. By using an API similar to GoogleTest, this module aims to bring
a comprehensive and intuitive testing framework to our users, much like
:ref:`module-pw_unit_test`.

.. warning::
  The PW_PERF_TEST macro is still under construction and should not be relied
  upon yet

-------------------
Perf Test Interface
-------------------
The user experience of writing a performance test is intended to be as
friction-less as possible. With the goal of being used for micro-benchmarking
code, writing a performance test is as easy as:

.. code-block:: cpp

  void TestFunction(::pw::perf_test::State& state){
    // space to create any needed variables.
    while (state.KeepRunning()){
      // code to measure here
    }
  }
  PW_PERF_TEST(PerformanceTestName, TestFunction);

However, it is recommended to read this guide to understand and write tests that
are suited towards your platform and the type of code you are trying to
benchmark.

State
=====
Within the testing framework, the state object is responsible for calling the
timing interface and keeping track of testing iterations. It contains only one
publicly accessible function, since the object is intended for internal use
only. The ``KeepRunning()`` function collects timestamps to measure the code
and ensures that only a certain number of iterations are run. To use the state
object properly, pass it as an argument of the test function and pass in the
``KeepRunning()`` function as the condition in a ``while()`` loop. The
``KeepRunning()`` function collects timestamps to measure the code and ensures
that only a certain number of iterations are run. Therefore the code to be
measured should be in the body of the ``while()`` loop like so:

.. code-block:: cpp

  // The State object is injected into a performance test by including it as an
  // argument to the function.
  void TestFunction(::pw::perf_test::State& state_obj){
    while (state_obj.KeepRunning()){
      /*
        Code to be measured here
      */
    }
  }

Macro Interface
===============
The test collection and registration process is done by a macro, much like
:ref:`module-pw_unit_test`.

.. c:macro:: PW_PERF_TEST(test_name, test_function, ...)

  Registers a performance test. Any additional arguments are passed to the test
  function.

.. code-block:: cpp

  // Declare performance test functions.
  // The first argument is the state, which is passed in by the test framework.
  void TestFunction(pw::perf_test::State& state) {
    // Test set up code
    Items a[] = {1, 2, 3};

    // Tests a KeepRunning() function, similar to Fuchsia's Perftest.
    while (state.KeepRunning()) {
      // Code under test, ran for multiple iterations.
      DoStuffToItems(a);
    }
  }

  void TestFunctionWithArgs(pw::perf_test::State& state, int arg1, bool arg2) {
    // Test set up code
    Thing object_created_outside(arg1);

    while (state.KeepRunning()) {
      // Code under test, ran for multiple iterations.
      object_created_outside.Do(arg2);
    }
  }

  // Tests are declared with any callable object. This is similar to Benchmark's
  // BENCMARK_CAPTURE() macro.
  PW_PERF_TEST(Name1, [](pw::perf_test::State& state) {
        TestFunctionWithArgs(1, false);
      })

  PW_PERF_TEST(Name2, TestFunctionWithArgs, 1, true);
  PW_PERF_TEST(Name3, TestFunctionWithArgs, 2, false);

.. warning::
  Internally, the testing framework stores the testing function as a function
  pointer. Therefore the test function argument must be converible to a function pointer

Event Handler
=============
The performance testing framework relies heavily on the member functions of
EventHandler to report iterations, the beginning of tests and other useful
information. The ``EventHandler`` class is a virtual interface meant to be
overridden, in order to provide flexibility on how data gets transferred.

.. cpp:class:: pw::perf_test::EventHandler

  Handles events from a performance test.

  .. cpp:function:: virtual void RunAllTestsStart(const TestRunInfo& summary)

    Called before all tests are run

  .. cpp:function:: virtual void RunAllTestsEnd()

    Called after all tests are run

  .. cpp:function:: virtual void TestCaseStart(const TestCase& info)

    Called when a new performance test is started

  .. cpp:function:: virtual void TestCaseEnd(const TestCase& info, const Results& end_result)

    Called after a performance test ends

  .. cpp:function:: virtual void ReportIteration(const IterationResult& result)

    Called to output the results of an iteration

Logging Event Handler
---------------------
The default method of running performance tests is using the
``LoggingEventHandler``. This event handler only logs the test results to the
console and nothing more. It was chosen as the default method due to its
portability and to cut down on the time it would take to implement other
printing log handlers. Make sure to set a ``pw_log`` backend.

Timing API
==========
In order to provide meaningful performance timings for given functions, events,
etc a timing interface must be implemented from scratch to be able to provide
for the testing needs. The timing API meets these needs by implementing either
clock cycle record keeping or second based recordings.

Time-Based Measurement
----------------------
For most host applications, pw_perf_test depends on :ref:`module-pw_chrono` its
timing needs. At the moment, the interface will only measure performance in
terms of nanoseconds. To see more information about how pw_chrono works, see the
module documentation.

Cycle Count Measurement
------------------------------------
In the case of running tests on an embedded system, clock cycles may give more
insight into the actual performance of the system. The timing API gives you this
option by providing time measurements through a facade. In this case, by setting
the ccynt timer as the backend, perf tests can be measured in clock cycles for
ARM Cortex devices.

This implementation directly accesses the registers of the Cortex, and therefore
needs no operating system to function. This is achieved by enabling the
`DWT register<https://developer.arm.com/documentation/ddi0337/e/System-Debug/DWT?lang=en>_`
through the `DEMCR register<https://developer.arm.com/documentation/ddi0337/e/CEGHJDCF>_`.
While this provides cycle counts directly from the CPU, notably it is vulnerable
to rollover upon a duration of a test exceeding 2^32 clock cycles. This works
out to a 43 second duration limit per iteration at 100 mhz.

.. warning::
  The interface only measures raw clock cycles and does not take into account
  other possible sources of pollution such as LSUs, Sleeps and other registers.
  `Read more on the DWT methods of counting instructions. <https://developer.arm.com/documentation/ka001499/1-0/>`_
