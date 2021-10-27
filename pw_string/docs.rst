.. _module-pw_string:

=========
pw_string
=========
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

-------------
Compatibility
-------------
C++17

-----
Usage
-----
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

Safe Length Checking
====================
This module provides two safer alternatives to ``std::strlen`` in case the
string is extremely long and/or potentially not null-terminated.

First, a constexpr alternative to C11's ``strnlen_s`` is offerred through
:cpp:func:`pw::string::ClampedCString`. This does not return a length by
design and instead returns a string_view which does not require
null-termination.

Second, a constexpr specialized form is offered where null termination is
required through :cpp:func:`pw::string::NullTerminatedLength`. This will only
return a length if the string is null-terminated.

.. cpp:function:: constexpr std::string_view pw::string::ClampedCString(std::span<const char> str)
.. cpp:function:: constexpr std::string_view pw::string::ClampedCString(const char* str, size_t max_len)

   Safe alternative to the string_view constructor to avoid the risk of an
   unbounded implicit or explicit use of strlen.

   This is strongly recommended over using something like C11's strnlen_s as
   a string_view does not require null-termination.

.. cpp:function:: constexpr pw::Result<size_t> pw::string::NullTerminatedLength(std::span<const char> str)
.. cpp:function:: pw::Result<size_t> pw::string::NullTerminatedLength(const char* str, size_t max_len)

   Safe alternative to strlen to calculate the null-terminated length of the
   string within the specified span, excluding the null terminator. Like C11's
   strnlen_s, the scan for the null-terminator is bounded.

   Returns:
     null-terminated length of the string excluding the null terminator.
     OutOfRange - if the string is not null-terminated.

   Precondition: The string shall be at a valid pointer.

pw::string::Copy
================
The ``pw::string::Copy`` functions provide a safer alternative to
``std::strncpy`` as it always null-terminates whenever the destination
buffer has a non-zero size.

.. cpp:function:: StatusWithSize Copy(const std::string_view& source, std::span<char> dest)
.. cpp:function:: StatusWithSize Copy(const char* source, std::span<char> dest)
.. cpp:function:: StatusWithSize Copy(const char* source, char* dest, size_t num)

   Copies the source string to the dest, truncating if the full string does not
   fit. Always null terminates if dest.size() or num > 0.

   Returns the number of characters written, excluding the null terminator. If
   the string is truncated, the status is ResourceExhausted.

   Precondition: The destination and source shall not overlap.
   Precondition: The source shall be a valid pointer.

pw::StringBuilder
=================
``pw::StringBuilder`` facilitates building formatted strings in a fixed-size
buffer. It is designed to give the flexibility of ``std::string`` and
``std::ostringstream``, but with a small footprint.

.. code-block:: cpp

  #include "pw_log/log.h"
  #include "pw_string/string_builder.h"

  pw::Status LogProducedData(std::string_view func_name,
                             std::span<const std::byte> data) {
    pw::StringBuffer<42> sb;

    // Append a std::string_view to the buffer.
    sb << func_name;

    // Append a format string to the buffer.
    sb.Format(" produced %d bytes of data: ", static_cast<int>(data.data()));

    // Append bytes as hex to the buffer.
    sb << data;

    // Log the final string.
    PW_LOG_DEBUG("%s", sb.c_str());

    // Errors encountered while mutating the string builder are tracked.
    return sb.status();
  }

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
    return Copy(MyStatusString(value), buffer);
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

-----------
Future work
-----------
* StringBuilder's fixed size cost can be dramatically reduced by limiting
  support for 64-bit integers.
* Consider integrating with the tokenizer module.

Zephyr
======
To enable ``pw_string`` for Zephyr add ``CONFIG_PIGWEED_STRING=y`` to the
project's configuration.
