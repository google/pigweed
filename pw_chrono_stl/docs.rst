.. _module-pw_chrono_stl:

-------------
pw_chrono_stl
-------------
``pw_chrono_stl`` is a collection of ``pw_chrono`` backends that are implemented
using STL's ``std::chrono`` library.

.. warning::
  This module is under construction, not ready for use, and the documentation
  is incomplete.

SystemClock backend
-------------------
The STL based ``system_clock`` backend implements the ``pw_chrono:system_clock``
facade by using the ``std::chrono::steady_clock``. Note that the
``std::chrono::system_clock`` cannot be used as this is not always a monotonic
clock source.

See the documentation for ``pw_chrono`` for further details.

Build targets
-------------
The GN build for ``pw_chrono_stl`` has one target: ``system_clock``.
The ``system_clock`` target provides the
``pw_chrono_backend/system_clock_config.h`` and
``pw_chrono_backend/system_clock_inline.h`` headers and the backend for the
``pw_chrono:system_clock``.
