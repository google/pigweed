.. _module-pw_string-api:

=======================
pw_string API reference
=======================

.. _pw_inlinebasicstring-api:

---------------------
pw::InlineBasicString
---------------------
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

.. cpp:class:: template <typename T, unsigned short kCapacity> pw::InlineBasicString

   Represents a fixed-capacity string of a generic character type. Equivalent to
   ``std::basic_string<T>``. Always null (``T()``) terminated.

----------------
pw::InlineString
----------------
See also :ref:`pw_inlinebasicstring-api`.

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
The ``pw::string::Copy`` functions provide a safer alternative to
``std::strncpy`` as it always null-terminates whenever the destination
buffer has a non-zero size.

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
The ``pw::string::PrintableCopy`` function provides a safe printable copy of a
string. It functions with the same safety of ``pw::string::Copy`` while also
converting any non-printable characters to a ``.`` char.

.. cpp:function:: StatusWithSize PrintableCopy(const std::string_view& source, span<char> dest)

-----------------
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
