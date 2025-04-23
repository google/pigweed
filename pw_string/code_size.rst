.. _module-pw_string-size-reports:

==================
Code Size Analysis
==================
.. pigweed-module-subpage::
   :name: pw_string

Save code space by replacing ``snprintf``
=========================================
The C standard library function ``snprintf`` is commonly used for string
formatting. However, it isn't optimized for embedded systems, and using it will
bring in a lot of other standard library code that will inflate your binary
size.

Size comparison: snprintf versus pw::StringBuilder
--------------------------------------------------
The fixed code size cost of :cpp:type:`pw::StringBuilder` is smaller than
that of ``std::snprintf``. Using only :cpp:type:`pw::StringBuilder`'s ``<<`` and
``append`` methods instead of ``snprintf`` leads to significant code size
reductions.

However, there are cases when the incremental code size cost of
:cpp:type:`pw::StringBuilder` is similar to that of ``snprintf``. For example,
each argument to :cpp:type:`pw::StringBuilder`'s ``<<`` method expands to a
function call, but one or two :cpp:type:`pw::StringBuilder` appends may still
have a smaller code size impact than a single ``snprintf`` call. Using
:cpp:type:`pw::StringBuilder` error handling will also impact code size in a
way that is comparable to ``snprintf``.

.. include:: string_builder_size_report

Size comparison: snprintf versus pw::string::Format
---------------------------------------------------
The ``pw::string::Format`` functions have a small, fixed code size
cost. However, relative to equivalent ``std::snprintf`` calls, there is no
incremental code size cost to using ``pw::string::Format``.

.. include:: format_size_report
