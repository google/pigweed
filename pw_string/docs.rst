.. _module-pw_string:

---------
pw_string
---------
String manipulation is a very common operation, but the standard C and C++
string libraries have drawbacks. The C++ functions are easy-to-use and powerful,
but require too much flash and memory for many embedded projects. The C string
functions are lighter weight, but can be difficult to use correctly. Mishandling
of null terminators or buffer sizes can result in serious bugs.

The ``pw_string`` module provides the flexibility, ease-of-use, and safety of
C++-style string manipulation, but with no dynamic memory allocation and a much
smaller binary size impact. Using ``pw_string`` in place of the standard C
functions eliminates issues related to buffer overflow or missing null
terminators.

Compatibility
=============
C++17

pw::string::Format
==================
The ``pw::string::Format`` and ``pw::string::FormatVaList`` functions provide
safer alternatives to ``std::snprintf`` and ``std::vsnprintf``. The snprintf
return value is awkward to interpret, and misinterpreting it can lead to serious
bugs.

Size report: replacing snprintf with pw::string::Format
-------------------------------------------------------
The ``Format`` functions have a small, fixed code size cost. However, relative
to equivalent ``std::snprintf`` calls, there is no incremental code size cost to
using ``Format``.

.. include:: format_size_report

pw::StringBuilder
=================
``pw::StringBuilder`` facilitates building formatted strings in a fixed-size
buffer. It is designed to give the flexibility of ``std::string`` and
``std::ostringstream``, but with a small footprint.

Supporting custom types with StringBuilder
------------------------------------------
As with ``std::ostream``, StringBuilder supports printing custom types by
overriding the ``<<`` operator. This is is done by defining ``operator<<`` in
the same namespace as the custom type. For example:

.. code-block:: cpp

  namespace my_project {

  struct MyType {
    int foo;
    const char* bar;
  };

  pw::StringBuilder& operator<<(pw::StringBuilder& sb, const MyType& value) {
    return sb << "MyType(" << value.foo << ", " << value.bar << ')';
  }

  }  // namespace my_project

Internally, ``StringBuilder`` uses the ``ToString`` function to print. The
``ToString`` template function can be specialized to support custom types with
``StringBuilder``, though it is recommended to overload ``operator<<`` instead.
This example shows how to specialize ``pw::ToString``:

.. code-block:: cpp

  #include "pw_string/to_string.h"

  namespace pw {

  template <>
  StatusWithSize ToString<MyStatus>(MyStatus value, std::span<char> buffer) {
    return CopyString(MyStatusString(value), buffer);
  }

  }  // namespace pw

Size report: replacing snprintf with pw::StringBuilder
------------------------------------------------------
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

Future work
===========
* StringBuilder's fixed size cost can be dramatically reduced by limiting
  support for 64-bit integers.
* Consider integrating with the tokenizer module.
