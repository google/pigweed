.. _module-pw_async2-size-reports:

==================
Code size analysis
==================
.. pigweed-module-subpage::
   :name: pw_async2

--------------------------
Core async2 implementation
--------------------------
The following table shows the code size cost of adding ``pw_async2`` to a
system. These size reports assume a baseline system with an RTOS which already
uses a handful of core Pigweed components including HAL abstractions and
``pw_allocator.``

The first row captures the core of ``pw_async2``: the dispatcher, tasks,
and wakers, using the ``pw_async2_basic`` dispatcher backend. This is the
minimum size cost a system must pay to adopt ``pw_async2``. The following row
demonstrates the cost of adding another task to this system. Of course, the
majority of the cost of the task exists within its implementation --- this
simply shows that there is minimal internal overhead.

.. include:: size_report/full_size_report

----------------
async2 utilities
----------------
Pigweed provides several utilities to simplify writing asynchronous code. Among
these are combinators which operate over several pendables, such as ``Join``
which waits for all pendables to complete, and ``Select`` which waits for the
first pendable to complete.

The table below demonstrates the code size impact of using these utilities.
For both ``Join`` and ``Select``, the report shows:

*  The initial cost of using the utility with multiple pendables of the same
   type.
*  The incremental cost of adding a second call with pendables of *different*
   types, which demonstrates the overhead of template specialization.

Additionally, the table includes a comparison showing the code size difference
between using the ``Select`` helper versus manually polling each pendable.

.. include:: size_report/utilities_size_report

---------------------------
OnceSender and OnceReceiver
---------------------------
The next table shows sizes of the pair of ``OnceSender`` and ``OnceReceiver``
types, which allow for returning a delayed result from an async function,
similar to a ``Future`` type in other languages. This type is templated on its
stored value, causing specialization overhead for each type sent through the
sender/receiver pair. The first row showcases the base cost of using a
``OnceSender`` and ``OnceReceiver``; the second row adds another template
specialization on top of this to demonstrate the incremental cost.

.. include:: size_report/once_sender_size_report
