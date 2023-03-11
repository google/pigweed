.. _module-pw_string-api:

=============
API Reference
=============

--------
Overview
--------
This module provides two types of strings, and utility functions for working
with strings.

**pw::StringBuilder**

.. doxygenfile:: pw_string/string_builder.h
   :sections: briefdescription

**pw::InlineString**

.. doxygenfile:: pw_string/string.h
   :sections: briefdescription

**String utility functions**

.. doxygenfile:: pw_string/util.h
   :sections: briefdescription

-----------------
pw::StringBuilder
-----------------
.. doxygenfile:: pw_string/string_builder.h
   :sections: briefdescription
.. doxygenclass:: pw::StringBuilder
   :members:

----------------
pw::InlineString
----------------
.. doxygenfile:: pw_string/string.h
   :sections: detaileddescription

.. doxygenclass:: pw::InlineBasicString
   :members:

.. doxygentypedef:: pw::InlineString

------------------------
String utility functions
------------------------

pw::string::Assign()
--------------------
.. doxygenfunction:: pw::string::Assign(InlineString<> &string, const std::string_view &view)

pw::string::Append()
--------------------
.. doxygenfunction:: pw::string::Append(InlineString<>& string, const std::string_view& view)

pw::string::ClampedCString()
----------------------------
.. doxygenfunction:: pw::string::ClampedCString(const char* str, size_t max_len)
.. doxygenfunction:: pw::string::ClampedCString(span<const char> str)

pw::string::Copy()
------------------
.. doxygenfunction:: pw::string::Copy(const char* source, char* dest, size_t num)
.. doxygenfunction:: pw::string::Copy(const char* source, Span&& dest)
.. doxygenfunction:: pw::string::Copy(const std::string_view& source, Span&& dest)

It also has variants that provide a destination of ``pw::Vector<char>``
(see :ref:`module-pw_containers` for details) that do not store the null
terminator in the vector.

.. cpp:function:: StatusWithSize Copy(const std::string_view& source, pw::Vector<char>& dest)
.. cpp:function:: StatusWithSize Copy(const char* source, pw::Vector<char>& dest)

pw::string::Format()
--------------------
.. doxygenfile:: pw_string/format.h
   :sections: briefdescription
.. doxygenfunction:: pw::string::Format
.. doxygenfunction:: pw::string::FormatVaList

pw::string::NullTerminatedLength()
----------------------------------
.. doxygenfunction:: pw::string::NullTerminatedLength(const char* str, size_t max_len)
.. doxygenfunction:: pw::string::NullTerminatedLength(span<const char> str)

pw::string::PrintableCopy()
---------------------------
.. doxygenfunction:: pw::string::PrintableCopy(const std::string_view& source, span<char> dest)
