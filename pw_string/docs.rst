.. _chapter-string:

.. default-domain:: cpp

.. highlight:: sh

---------
pw_string
---------
The string module provides utilities for working with strings.

Requirements
=============
C++17

Module Dependencies
-------------------
* pw_preprocessor
* pw_status
* pw_span

Features
========

pw::string::Format
------------------
The ``pw::string::Format`` functions provides are safer alternatives to
``std::snprintf`` and ``std::vsnprintf``. The snprintf return value is awkward
to interpret, and misinterpreting it can lead to serious bugs.

Size report: replacing snprintf with pw::string::Format
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The ``Format`` functions have a small, fixed code size cost. However, relative
to equivalent ``std::snprintf`` calls, there is no incremental code size cost to
using ``Format``.

.. include:: format_size_report.rst

pw::StringBuilder
-----------------
StringBuilder facilitates building formatted strings in a fixed-size buffer.
StringBuilder is safe, flexible, and results in much smaller code size than
using std::ostringstream. However, applications sensitive to code size should
use StringBuilder with care.

Size report: replacing snprintf with pw::StringBuilder
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The fixed code size cost of StringBuilder is significant, though smaller than
std::snprintf. Using StringBuilder's << and append methods exclusively in
place of snprintf reduces code size, but snprintf may be difficult to avoid.

The incremental code size cost of StringBuilder is comparable to snprintf if
errors are handled. Each argument to StringBuilder's << expands to a function
call, but one or two StringBuilder appends may have a smaller code size
impact than a single snprintf call.

.. include:: string_builder_size_report.rst

Future work
^^^^^^^^^^^
* StringBuilder's fixed size cost can be dramatically reduced by limiting
  support for 64-bit integers.
* Consider integrating with the tokenizer module.
