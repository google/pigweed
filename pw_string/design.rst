.. _module-pw_string-design:

================
Design & Roadmap
================
.. pigweed-module-subpage::
   :name: pw_string

``pw_string`` provides string classes and utility functions designed to
prioritize safety and static allocation. The APIs are broadly similar to those
of the string classes in the C++ standard library, so familiarity with those
classes will provide some context around ``pw_string`` design decisions.

.. _module-pw_string-design-inlinestring:

--------------------------
Design of pw::InlineString
--------------------------
:cpp:type:`pw::InlineString` / :cpp:class:`pw::InlineBasicString` are designed
to match the ``std::string`` / ``std::basic_string<T>`` API as closely as
possible, but with key differences to improve performance on embedded systems:

- **Fixed capacity:** Operations that add characters to the string beyond its
  capacity are an error. These trigger a ``PW_ASSERT`` at runtime. When
  detectable, these situations trigger a ``static_assert`` at compile time.
- **Minimal overhead:** :cpp:type:`pw::InlineString` operations never
  allocate. Reading the contents of the string is a direct memory access within
  the string object, without pointer indirection.
- **Constexpr support:** :cpp:type:`pw::InlineString` works in ``constexpr``
  contexts, which is not supported by ``std::string`` until C++20.

We don't aim to provide complete API compatibility with
``std::string`` / ``std::basic_string<T>``. Some areas of deviation include:

- **Compile-time capacity checks:** :cpp:type:`InlineString` provides overloads
  specific to character arrays. These perform compile-time capacity checks and
  are used for class template argument deduction.
- **Implicit conversions from** ``std::string_view`` **:** Specifying the
  capacity parameter is cumbersome, so implicit conversions are helpful. Also,
  implicitly creating a :cpp:type:`InlineString` is less costly than creating a
  ``std::string``. As with ``std::string``, explicit conversions are required
  from types that convert to ``std::string_view``.
- **No dynamic allocation functions:** Functions that allocate memory, like
  ``reserve()``, ``shrink_to_fit()``, and ``get_allocator()``, are simply not
  present.

Capacity
========
:cpp:type:`InlineBasicString` has a template parameter for the capacity, but the
capacity does not need to be known by the user to use the string safely. The
:cpp:type:`InlineBasicString` template inherits from a
:cpp:type:`InlineBasicString` specialization with capacity of the reserved value
``pw::InlineString<>::npos``. The actual capacity is stored in a single word
alongside the size. This allows code to work with strings of any capacity
through a ``InlineString<>`` or ``InlineBasicString<T>`` reference.

Exceeding the capacity
----------------------
Any :cpp:type:`pw::InlineString` operations that exceed the string's capacity
fail an assertion, resulting in a crash. Helpers are provided in
``pw_string/util.h`` that return ``pw::Status::ResourceExhausted()`` instead of
failing an assert when the capacity would be exceeded.

----------------------------------
Design of string utility functions
----------------------------------

Safe length checking
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

.. _module-pw_string-roadmap:

-------
Roadmap
-------
* The fixed size cost of :cpp:type:`pw::StringBuilder` can be dramatically
  reduced by limiting support for 64-bit integers.
* ``pw_string`` may be integrated with :ref:`module-pw_tokenizer`.
