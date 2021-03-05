.. _module-pw_thread_threadx:

=================
pw_thread_threadx
=================
This is a set of backends for pw_thread based on ThreadX.

.. Warning::
  This module is still under construction, the API is not yet stable.

.. list-table::

  * - :ref:`module-pw_thread` Facade
    - Backend Target
    - Description
  * - ``pw_thread:id``
    - ``pw_thread_threadx:id``
    - Thread identification.
  * - ``pw_thread:yield``
    - ``pw_thread_threadx:yield``
    - Thread scheduler yielding.
  * - ``pw_thread:sleep``
    - ``pw_thread_threadx:sleep``
    - Thread scheduler sleeping.
  * - ``pw_thread:thread``
    - ``pw_thread_threadx:thread``
    - Thread creation.
