.. _module-pw_string:

===========
pw_string
===========
..
  A short description of the module. Aim for three sentences at most that clearly
  convey what the module does.

.. card::

   :octicon:`comment-discussion` Status:
   :bdg-secondary-line:`Experimental`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Unstable`
   :octicon:`chevron-right`
   :bdg-primary:`Stable`
   :octicon:`kebab-horizontal`
   :bdg-primary:`Current`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Deprecated`

----------
Background
----------
..
  Describe *what* problem this module solves. You can make references to other
  existing solutions to highlight why their approaches were not suitable, but
  avoid describing *how* this module solves the problem in this section.

String manipulation is a very common operation, but the standard C and C++
string libraries have drawbacks. The C++ functions are easy-to-use and powerful,
but require too much flash and memory for many embedded projects. The C string
functions are lighter weight, but can be difficult to use correctly. Mishandling
of null terminators or buffer sizes can result in serious bugs.

------------
Our solution
------------
..
  Describe *how* this module addresses the problems highlighted in section above.
  This is a great place to include artifacts other than text to more effectively
  deliver the message. For example, consider including images or animations for
  UI-related modules, or code samples illustrating a superior API in libraries.

The ``pw_string`` module provides the flexibility, ease-of-use, and safety of
C++-style string manipulation, but with no dynamic memory allocation and a much
smaller binary size impact. Using ``pw_string`` in place of the standard C
functions eliminates issues related to buffer overflow or missing null
terminators.

pw::InlineString
----------------
:cpp:class:`pw::InlineBasicString` and :cpp:type:`pw::InlineString` are
C++14-compatible, fixed-capacity, null-terminated string classes. They are
equivalent to ``std::basic_string<T>`` and ``std::string``, but store the string
contents inline and use no dynamic memory.

:cpp:type:`pw::InlineString` takes the fixed capacity as a template argument,
but may be used generically without specifying the capacity. The capacity value
is stored in a member variable, which the generic ``pw::InlineString<>`` /
``pw::InlineBasicString<T>`` specialization uses in place of the template
parameter.

:cpp:type:`pw::InlineString` is efficient and compact. The current size and
capacity are stored in a single word. Accessing the contents of a
:cpp:type:`pw::InlineString` is a simple array access within the object, with no
pointer indirection, even when working from a generic ``pw::InlineString<>``
reference.

pw::StringBuilder
-----------------
.. cpp:namespace-push:: pw::StringBuilder

:cpp:class:`StringBuilder` facilitates creating formatted strings in a
fixed-sized buffer or :cpp:type:`pw::InlineString`. It is designed to give the
flexibility of ``std::ostringstream``, but with a small footprint.

:cpp:class:`StringBuilder` supports C++ ``<<``-style output, printf formatting,
and a few ``std::string`` functions (:cpp:func:`append()`,
:cpp:func:`push_back()`, :cpp:func:`pop_back`.

.. cpp:namespace-pop::

.. doxygenclass:: pw::StringBuilder
   :members:


pw::string::PrintableCopy
-------------------------
The ``pw::string::PrintableCopy`` function provides a safe printable copy of a
string. It functions with the same safety of ``pw::string::Copy`` while also
converting any non-printable characters to a ``.`` char.


pw::string::Copy
----------------
The ``pw::string::Copy`` functions provide a safer alternative to
``std::strncpy`` as it always null-terminates whenever the destination
buffer has a non-zero size.


pw::string::Format
------------------
The ``pw::string::Format`` and ``pw::string::FormatVaList`` functions provide
safer alternatives to ``std::snprintf`` and ``std::vsnprintf``. The snprintf
return value is awkward to interpret, and misinterpreting it can lead to serious
bugs.

---------------------
Design considerations
---------------------
``pw_string`` is designed to prioritize safety and static allocation. It matches
the ``std::string`` API as closely as possible, but isn't intended to provide
complete API compatibility. See the `design doc <design.rst>`_ for more details.

..
  Briefly explain any pertinent design considerations and trade-offs. For example,
  perhaps this module optimizes for very small RAM and Flash usage at the expense
  of additional CPU time, where other solutions might make a different choice.

  Many modules will have more extensive documentation on design and implementation
  details. If so, that additional content should be in ``design.rst`` and linked
  to from this section.

Size reports
------------
..
  If this module includes code that runs on target, include the size report(s)
  here.

Replacing snprintf with pw::StringBuilder
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
StringBuilder is safe, flexible, and results in much smaller code size than
using ``std::ostringstream``. However, applications sensitive to code size
should use StringBuilder with care.

The fixed code size cost of StringBuilder is significant, though smaller than
``std::snprintf``. Using StringBuilder's << and append methods exclusively in
place of ``snprintf`` reduces code size, but ``snprintf`` may be difficult to
avoid.

The incremental code size cost of StringBuilder is comparable to ``snprintf`` if
errors are handled. Each argument to StringBuilder's ``<<`` expands to a
function call, but one or two StringBuilder appends may have a smaller code size
impact than a single ``snprintf`` call.

.. include:: string_builder_size_report

Replacing snprintf with pw::string::Format
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The ``Format`` functions have a small, fixed code size cost. However, relative
to equivalent ``std::snprintf`` calls, there is no incremental code size cost to
using ``Format``.

.. include:: format_size_report

Roadmap
-------
* StringBuilder's fixed size cost can be dramatically reduced by limiting
  support for 64-bit integers.
* Consider integrating with the tokenizer module.

---------------
Who this is for
---------------
..
  Highlight the use cases that are appropriate for this module. This is the place
  to make the sales pitch-why should developers use this module, and what makes
  this module stand out from alternative solutions that try to address this
  problem.

--------------------
Is it right for you?
--------------------
..
  This module may not solve all problems or be appropriate for all circumstances.
  This is the place to add those caveats. Highly-experimental modules that are
  under very active development might be a poor fit for any developer right now.
  If so, mention that here.

..
  https://issues.pigweed.dev/issues/269671709

Compatibility
-------------
C++17, C++14 (:cpp:type:`pw::InlineString`)

---------------
Getting started
---------------
..
  Explain how developers use this module in their project, including any
  prerequisites and conditions. This section should cover the fastest and simplest
  ways to get started (for example, "add this dependency in GN", "include this
  header"). More complex and specific use cases should be covered in guides that
  are linked in this section.

Module Configuration Options
----------------------------
The following configuration options can be adjusted via compile-time
configuration of this module.

.. c:macro:: PW_STRING_ENABLE_DECIMAL_FLOAT_EXPANSION

   Setting this to a non-zero value will result in the ``ToString`` function
   outputting string representations of floating-point values with a decimal
   expansion after the point, by using the ``Format`` function. The default
   value of this configuration option is zero, which will result in floating
   point values being rounded to the nearest integer in their string
   representation.

   Using a non-zero value for this configuration option may incur a code size
   cost due to the dependency on ``Format``.

Zephyr
------
To enable ``pw_string`` for Zephyr add ``CONFIG_PIGWEED_STRING=y`` to the
project's configuration.

--------
Contents
--------
.. toctree::
   :maxdepth: 1

   design
   guide
   api
