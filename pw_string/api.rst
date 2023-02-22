.. _module-pw_string-api:

=======================
pw_string API reference
=======================
..
  If a module includes a developer-facing API, this is the place to include API
  documentation.

  Note that Pigweed does not favor fully-automated API documentation generation.
  While we welcome generating documentation from code, curation and thoughtful
  developer intent are necessary. Some guidance:

  * Only include API documentation for parts of the API that are valuable to
    expose in documentation.

  * Do not include generated API data for any functions, classes, etc., that lack
    their own documentation, like docstrings or additional surrounding commentary
    in the containing doc.

  A module may have APIs for multiple languages. If so, replace this file with an
  `api` directory and an `index.rst` file that links to separate docs for each
  supported language.

---------------------
pw::InlineBasicString
---------------------
.. cpp:class:: template <typename T, unsigned short kCapacity> pw::InlineBasicString

   Represents a fixed-capacity string of a generic character type. Equivalent to
   ``std::basic_string<T>``. Always null (``T()``) terminated.

.. cpp:type:: template <unsigned short kCapacity> pw::InlineString = pw::InlineBasicString<char, kCapacity>

   Represents a fixed-capacity string of ``char`` characters. Equivalent to
   ``std::string``. Always null (``'\0'``) terminated.

----------
pw::string
----------

pw::string::Assign
------------------
.. cpp:function:: pw::Status pw::string::Assign(pw::InlineString<>& string, const std::string_view& view)

   Assigns ``view`` to ``string``. Truncates and returns ``RESOURCE_EXHAUSTED``
   if ``view`` is too large for ``string``.

pw::string::Append
------------------
.. cpp:function:: pw::Status pw::string::Append(pw::InlineString<>& string, const std::string_view& view)

   Appends ``view`` to ``string``. Truncates and returns ``RESOURCE_EXHAUSTED``
   if ``view`` does not fit within the remaining capacity of ``string``.

pw::string::ClampedCString
--------------------------
.. cpp:function:: constexpr std::string_view pw::string::ClampedCString(span<const char> str)
.. cpp:function:: constexpr std::string_view pw::string::ClampedCString(const char* str, size_t max_len)

   Safe alternative to the string_view constructor to avoid the risk of an
   unbounded implicit or explicit use of strlen.

   This is strongly recommended over using something like C11's strnlen_s as
   a string_view does not require null-termination.

pw::string::Copy
----------------
.. cpp:function:: StatusWithSize Copy(const std::string_view& source, span<char> dest)
.. cpp:function:: StatusWithSize Copy(const char* source, span<char> dest)
.. cpp:function:: StatusWithSize Copy(const char* source, char* dest, size_t num)
.. cpp:function:: StatusWithSize Copy(const pw::Vector<char>& source, span<char> dest)

   Copies the source string to the dest, truncating if the full string does not
   fit. Always null terminates if dest.size() or num > 0.

   Returns the number of characters written, excluding the null terminator. If
   the string is truncated, the status is ResourceExhausted.

   Precondition: The destination and source shall not overlap.
   Precondition: The source shall be a valid pointer.

It also has variants that provide a destination of ``pw::Vector<char>``
(see :ref:`module-pw_containers` for details) that do not store the null
terminator in the vector.

.. cpp:function:: StatusWithSize Copy(const std::string_view& source, pw::Vector<char>& dest)
.. cpp:function:: StatusWithSize Copy(const char* source, pw::Vector<char>& dest)

pw::string::NullTerminatedLength
--------------------------------
.. cpp:function:: constexpr pw::Result<size_t> pw::string::NullTerminatedLength(span<const char> str)
.. cpp:function:: pw::Result<size_t> pw::string::NullTerminatedLength(const char* str, size_t max_len)

   Safe alternative to strlen to calculate the null-terminated length of the
   string within the specified span, excluding the null terminator. Like C11's
   strnlen_s, the scan for the null-terminator is bounded.

   Returns:
     null-terminated length of the string excluding the null terminator.
     OutOfRange - if the string is not null-terminated.

   Precondition: The string shall be at a valid pointer.

pw::string::PrintableCopy
-------------------------
.. cpp:function:: StatusWithSize PrintableCopy(const std::string_view& source, span<char> dest)

pw::StringBuilder
-----------------

