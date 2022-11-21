.. _module-pw_perf_test:

============
pw_perf_test
============
Pigweed's perf test module provides an easy way to measure performance on
any test setup. By using an API similar to GoogleTest, this module aims to bring
a robust and intuitive testing framework to our users.

.. note::
  This module is still under construction. Some features may not be available
  yet.

---------
Timer API
---------
In order to provide meaningful performance timings for given functions, events,
etc a timing interface must be implemented from scratch to be able to provide
for the testing needs. The timing API meets these needs by implementing either
clock cycle record keeping or second based recordings.

Time-Based Measurement
======================
In order to achieve time-based measurements, pw_perf_test depends on
:ref:`module-pw_chrono` its timing needs. At the moment, the interface will
only measure performance in terms of nanoseconds. To see more information about
how pw_chrono works, see the module documentation.

-----
State
-----
Within the testing framework, the state object is responsible for calling the
timing interface and keeping track of testing iterations. It contains only two
publicly accessible functions, since the object is intended for internal use
only. ``The SetIterations()`` function ensures that the user can specify the
amount of iterations if the default behavior is not satisfactory, while the
``KeepRunning()`` function collects timestamps for when parts of the code are
executed and ensures that only a certain number of iterations are run.

.. code-block:: cpp

  void TestFunction(::pw::perf_test::State& state){
    state.SetIterations(10);
    while(state_obj.KeepRunning()){
      // code to measure here
    }
  }
  PW_PERF_TEST(VariableIterationTest, TestFunction);


