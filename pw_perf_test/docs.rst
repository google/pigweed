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
for the testing needs. The timing API meets these needs by implementing
either clock cycle record keeping or second based recordings.
