.. _module-pw_string-guide:

===============
pw_string guide
===============

Building strings with StringBuilder
-----------------------------------
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

Constructing pw::InlineString objects
-------------------------------------
:cpp:type:`pw::InlineString` objects must be constructed by specifying a fixed
capacity for the string.

.. code-block:: c++

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

Choosing between InlineString and StringBuilder
-----------------------------------------------
:cpp:type:`pw::InlineString` is comparable to ``std::string``, while
:cpp:class:`pw::StringBuilder` is comparable to ``std::ostringstream``.
Because :cpp:class:`pw::StringBuilder` provides high-level stream functionality,
it has more overhead than :cpp:type:`pw::InlineString`.

Use :cpp:type:`pw::InlineString` unless :cpp:class:`pw::StringBuilder`'s
capabilities are needed. Features unique to :cpp:class:`pw::StringBuilder`
include:

* Polymorphic C++ stream-style output, potentially supporting custom types.
* Non-fatal handling of failed append/format operations.
* Tracking the status of a series of operations.
* Building a string in an external buffer.

If those features are not required, use :cpp:type:`pw::InlineString`. A common
example of when to prefer :cpp:type:`pw::InlineString` is wrapping a
length-delimited string (e.g. ``std::string_view``) for APIs that require null
termination.

.. code-block:: cpp

  void ProcessName(std::string_view name) {
    PW_LOG_DEBUG("The name is %s", pw::InlineString<kMaxNameLen>(name).c_str());

Operating on unknown size strings
---------------------------------
All :cpp:type:`pw::InlineString` operations may be performed on strings without
specifying their capacity.

.. code-block:: c++

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

Operating on known-size strings
-------------------------------
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

Building strings
----------------
:cpp:class:`pw::StringBuilder` may be used to build a string in a
:cpp:type:`pw::InlineString`.

Deducing class template arguments with pw::InlineBasicString
------------------------------------------------------------
:cpp:type:`pw::InlineBasicString` supports class template argument deduction
(CTAD) in C++17 and newer. Since :cpp:type:`pw::InlineString` is an alias, CTAD
is not supported until C++20.

.. code-block:: c++

   // Deduces a capacity of 5 characters to match the 5-character string literal
   // (not counting the null terminator).
   pw::InlineBasicString inline_string = "12345";

   // In C++20, CTAD may be used with the pw::InlineString alias.
   pw::InlineString my_other_string("123456789");


Printing custom types
---------------------
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
