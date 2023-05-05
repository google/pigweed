.. _module-pw_string-guide:

================
pw_string: Guide
================
.. pigweed-module-subpage::
   :name: pw_string
   :tagline: Efficient, easy, and safe string manipulation
   :nav:
    getting started: module-pw_string-get-started
    design: module-pw_string-design
    guides: module-pw_string-guide
    api: module-pw_string-api

InlineString and StringBuilder?
===============================
Use :cpp:type:`pw::InlineString` if you need:

* Compatibility with ``std::string``
* Storage internal to the object
* A string object to persist in other data structures
* Lower code size overhead

Use :cpp:class:`pw::StringBuilder` if you need:

* Compatibility with ``std::ostringstream``, including custom object support
* Storage external to the object
* Non-fatal handling of failed append/format operations
* Tracking of the status of a series of operations
* A temporary stack object to aid string construction
* Medium code size overhead

An example of when to prefer :cpp:type:`pw::InlineString` is wrapping a
length-delimited string (e.g. ``std::string_view``) for APIs that require null
termination:

.. code-block:: cpp

   #include <string>
   #include "pw_log/log.h"
   #include "pw_string/string_builder.h"

   void ProcessName(std::string_view name) {
     // %s format strings require null terminated strings, so create one on the
     // stack with size up to kMaxNameLen, copy the string view `name` contents
     // into it, add a null terminator, and log it.
     PW_LOG_DEBUG("The name is %s",
                  pw::InlineString<kMaxNameLen>(name).c_str());
   }

An example of when to prefer :cpp:class:`pw::StringBuilder` is when
constructing a string for external use.

.. code-block:: cpp

  #include "pw_string/string_builder.h"

  pw::Status FlushSensorValueToUart(int32_t sensor_value) {
    pw::StringBuffer<42> sb;
    sb << "Sensor value: ";
    sb << sensor_value;  // Formats as int.
    FlushCStringToUart(sb.c_str());

    if (!sb.status().ok) {
      format_error_metric.Increment();  // Track overflows.
    }
    return sb.status();
  }


Building strings with pw::StringBuilder
=======================================
The following shows basic use of a :cpp:class:`pw::StringBuilder`.

.. code-block:: cpp

  #include "pw_log/log.h"
  #include "pw_string/string_builder.h"

  pw::Status LogProducedData(std::string_view func_name,
                             span<const std::byte> data) {
    // pw::StringBuffer allocates a pw::StringBuilder with a built-in buffer.
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

Building strings with pw::InlineString
======================================
:cpp:type:`pw::InlineString` objects must be constructed by specifying a fixed
capacity for the string.

.. code-block:: c++

   #include "pw_string/string.h"

   // Initialize from a C string.
   pw::InlineString<32> inline_string = "Literally";
   inline_string.append('?', 3);   // contains "Literally???"

   // Supports copying into known-capacity strings.
   pw::InlineString<64> other = inline_string;

   // Supports various helpful std::string functions
   if (inline_string.starts_with("Lit") || inline_string == "not\0literally"sv) {
     other += inline_string;
   }

   // Like std::string, InlineString is always null terminated when accessed
   // through c_str(). InlineString can be used to null-terminate
   // length-delimited strings for APIs that expect null-terminated strings.
   std::string_view file(".gif");
   if (std::fopen(pw::InlineString<kMaxNameLen>(file).c_str(), "r") == nullptr) {
     return;
   }

   // pw::InlineString integrates well with std::string_view. It supports
   // implicit conversions to and from std::string_view.
   inline_string = std::string_view("not\0literally", 12);

   FunctionThatTakesAStringView(inline_string);

   FunctionThatTakesAnInlineString(std::string_view("1234", 4));

Building strings inside InlineString with a StringBuilder
=========================================================
:cpp:class:`pw::StringBuilder` can build a string in a
:cpp:type:`pw::InlineString`:

.. code-block:: c++

   #include "pw_string/string.h"

   void DoFoo() {
     InlineString<32> inline_str;
     StringBuilder sb(inline_str);
     sb << 123 << "456";
     // inline_str contains "456"
   }

Passing InlineStrings as parameters
===================================
:cpp:type:`pw::InlineString` objects can be passed to non-templated functions
via type erasure. This saves code size in most cases, since it avoids template
expansions triggered by string size differences.

Unknown size strings
--------------------
To operate on :cpp:type:`pw::InlineString` objects without knowing their type,
use the ``pw::InlineString<>`` type, shown in the examples below:

.. code-block:: c++

   // Note that the first argument is a generically-sized InlineString.
   void RemoveSuffix(pw::InlineString<>& string, std::string_view suffix) {
     if (string.ends_with(suffix)) {
        string.resize(string.size() - suffix.size());
     }
   }

   void DoStuff() {
     pw::InlineString<32> str1 = "Good morning!";
     RemoveSuffix(str1, " morning!");

     pw::InlineString<40> str2 = "Good";
     RemoveSuffix(str2, " morning!");

     PW_ASSERT(str1 == str2);
   }

However, generically sized :cpp:type:`pw::InlineString` objects don't work in
``constexpr`` contexts.

Known size strings
------------------
:cpp:type:`pw::InlineString` operations on known-size strings may be used in
``constexpr`` expressions.

.. code-block:: c++

   static constexpr pw::InlineString<64> kMyString = [] {
     pw::InlineString<64> string;

     for (int i = 0; i < 10; ++i) {
       string += "Hello";
     }

     return string;
   }();

Compact initialization of InlineStrings
=======================================
:cpp:type:`pw::InlineBasicString` supports class template argument deduction
(CTAD) in C++17 and newer. Since :cpp:type:`pw::InlineString` is an alias, CTAD
is not supported until C++20.

.. code-block:: c++

   // Deduces a capacity of 5 characters to match the 5-character string literal
   // (not counting the null terminator).
   pw::InlineBasicString inline_string = "12345";

   // In C++20, CTAD may be used with the pw::InlineString alias.
   pw::InlineString my_other_string("123456789");

Supporting custom types with StringBuilder
==========================================
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
  StatusWithSize ToString<MyStatus>(MyStatus value, span<char> buffer) {
    return Copy(MyStatusString(value), buffer);
  }

  }  // namespace pw
