.. _module-pw_function:

===========
pw_function
===========
.. pigweed-module::
   :name: pw_function
   :tagline: Embedded-friendly std::function
   :status: stable
   :languages: C++17

* **Familiar**. ``pw_function`` provides a standard, general-purpose API for
  wrapping callable objects that's similar to `std::function`_.
* **Optimized**. ``pw_function`` doesn't allocate (unless you want it to) and
  uses several tricks to prevent code bloat.

.. _std\:\:function: https://en.cppreference.com/w/cpp/utility/functional/function

.. code-block:: c++

   #include "pw_function/function.h"

   // pw::Function can be constructed from a function pointer...
   int _a(int a, int b) { return a + b; }
   pw::Function<int(int, int)> add(_a);
   // ... or a lambda.
   pw::Function<int(int)> square([](int num) { return num * num; });

   // pw::Callback can only be invoked once. After the first call, the target
   // function is released and destroyed, along with any resources owned by
   // that function.
   pw::Callback<void(void)> flip_table_once([](void) {
     // (╯°□°)╯︵ ┻━┻
   });

   add(5, 6);
   add = nullptr;  // pw::Function and pw::Callback are nullable
   add(7, 2);  // CRASH

   square(4);

   if (flip_table_once != nullptr) {  // Safe to call
     flip_table_once();
   } else {
     // ┬─┬ノ( º _ ºノ)
   }


.. _module-pw_function-start:

-----------
Get started
-----------
.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_function`` to your target's ``deps``:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_function",
             # ...
           ]
         }

      This assumes that your Bazel ``WORKSPACE`` has a `repository
      <https://bazel.build/concepts/build-ref#repositories>`_ named ``@pigweed``
      that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``$dir_pw_function`` to your target's ``deps``:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_function",
             # ...
           ]
         }

   .. tab-item:: CMake

      Link your library to ``pw_function``:

      .. code-block::

         add_library(my_lib ...)
         target_link_libraries(my_lib PUBLIC pw_function)

Use ``pw_function`` in your C++ code:

.. code-block:: c++

   #include "pw_function/function.h"

   // ...

.. _module-pw_function-guides:

------
Guides
------

Construct ``pw::Function`` from a function pointer
==================================================
:cpp:type:`pw::Function` is a move-only callable wrapper constructable from any
callable object. It's templated on the signature of the callable it stores and
implements the call operator; invoking a ``pw::Function`` object forwards to
the stored callable.

.. code-block:: c++

   int Add(int a, int b) { return a + b; }

   // Construct a Function object from a function pointer.
   pw::Function<int(int, int)> add_function(Add);

   // Invoke the function object.
   int result = add_function(3, 5);
   EXPECT_EQ(result, 8);

Construct ``pw::Function`` from a lambda
========================================
.. code-block:: c++

   // Construct a function from a lambda.
   pw::Function<int(int)> negate([](int value) { return -value; });
   EXPECT_EQ(negate(27), -27);

Create single-use functions with ``pw::Callback``
=================================================
:cpp:type:`pw::Callback` is a specialization of :cpp:type:`pw::Function` that
can only be called once. After a :cpp:type:`pw::Callback` is called, the target
function is released and destroyed, along with any resources owned by that
function. A :cpp:type:`pw::Callback` in the "already called" state
has the same state as a :cpp:type:`pw::Function` that has been assigned to
nullptr.

.. code-block:: cpp

   pw::Callback<void(void)> flip_table_once([](void) {
     // (╯°□°)╯︵ ┻━┻
   });

   flip_table_once();  // OK
   flip_table_once();  // CRASH

Nullifying functions and comparing to null
==========================================
``pw::Function`` and ``pw::Callback`` are nullable and can be compared to
``nullptr``. Invoking a null function triggers a runtime assert.

.. code-block:: c++

   // A function initialized without a callable is implicitly null.
   pw::Function<void()> null_function;

   // Null functions may also be explicitly created or set.
   pw::Function<void()> explicit_null_function(nullptr);

   pw::Function<void()> function([]() {});  // Valid (non-null) function.
   function = nullptr;  // Set to null, clearing the stored callable.

   // Functions are comparable to nullptr.
   if (function != nullptr) {
     function();
   }

``constexpr`` constructors and ``constinit`` expressions
========================================================
The default constructor for :cpp:type:`pw::Function` is ``constexpr``, so
default-constructed functions may be used in classes with ``constexpr``
constructors and in ``constinit`` expressions.

.. code-block:: c++

   class MyClass {
    public:
     // Default construction of a pw::Function is constexpr.
     constexpr MyClass() { ... }

     pw::Function<void(int)> my_function;
   };

   // pw::Function and classes that use it may be constant initialized.
   constinit MyClass instance;

``pw::Function`` as a function parameter
========================================
When implementing an API which uses callbacks, ``pw::Function`` can be used in
place of a function pointer or equivalent callable.

.. code-block:: c++

   // Before:
   void DoTheThing(int arg, void (*callback)(int result));

   // After:
   void DoTheThing(int arg, const pw::Function<void(int result)>& callback);
   // Note the parameter name within the function signature template for clarity.

.. _module-pw_function-move-semantics:

Move semantics
==============
:cpp:type:`pw::Function` is movable, but not copyable, so APIs must accept
:cpp:type:`pw::Function` objects either by const reference (``const
pw::Function<void()>&``) or rvalue reference (``const pw::Function<void()>&&``).
If the :cpp:type:`pw::Function` simply needs to be called, it should be passed
by const reference. If the :cpp:type:`pw::Function` needs to be stored, it
should be passed as an rvalue reference and moved into a
:cpp:type:`pw::Function` variable as appropriate.

.. code-block:: c++

   // This function calls a pw::Function but doesn't store it, so it takes a
   // const reference.
   void CallTheCallback(const pw::Function<void(int)>& callback) {
     callback(123);
   }

   // This function move-assigns a pw::Function to another variable, so it takes
   // an rvalue reference.
   void StoreTheCallback(pw::Function<void(int)>&& callback) {
     stored_callback_ = std::move(callback);
   }

.. admonition:: Rules of thumb for passing a :cpp:type:`pw::Function` to a function

   * **Pass by value**: Never.
     This results in unnecessary :cpp:type:`pw::Function` instances and move
     operations.

   * **Pass by const reference** (``const pw::Function&``): When the
     :cpp:type:`pw::Function` is only invoked.

     When a :cpp:type:`pw::Function` is called or inspected, but not moved, take
     a const reference to avoid copies and support temporaries.

   * **Pass by rvalue reference** (``pw::Function&&``): When the
     :cpp:type:`pw::Function` is moved.

     When the function takes ownership of the :cpp:type:`pw::Function` object,
     always use an rvalue reference (``pw::Function<void()>&&``) instead of a
     mutable lvalue reference (``pw::Function<void()>&``). An rvalue reference
     forces the caller to ``std::move`` when passing a preexisting
     :cpp:type:`pw::Function` variable, which makes the transfer of ownership
     explicit. It is possible to move-assign from an lvalue reference, but this
     fails to make it obvious to the caller that the object is no longer valid.

   * **Pass by non-const reference** (``pw::Function&``): Rarely, when modifying
     a variable.

     Non-const references are only necessary when modifying an existing
     :cpp:type:`pw::Function` variable. Use an rvalue reference instead if the
     :cpp:type:`pw::Function` is moved into another variable.

Calling functions that use ``pw::Function``
===========================================
A :cpp:type:`pw::Function` can be implicitly constructed from any callback
object. When calling an API that takes a :cpp:type:`pw::Function`, simply pass
the callable object.  There is no need to create an intermediate
:cpp:type:`pw::Function` object.

.. code-block:: c++

   // Implicitly creates a pw::Function from a capturing lambda and calls it.
   CallTheCallback([this](int result) { result_ = result; });

   // Implicitly creates a pw::Function from a capturing lambda and stores it.
   StoreTheCallback([this](int result) { result_ = result; });

When working with an existing :cpp:type:`pw::Function` variable, the variable
can be passed directly to functions that take a const reference. If the function
takes ownership of the :cpp:type:`pw::Function`, move the
:cpp:type:`pw::Function` variable at the call site.

.. code-block:: c++

   // Accepts the pw::Function by const reference.
   CallTheCallback(my_function_);

   // Takes ownership of the pw::Function.
   void StoreTheCallback(std::move(my_function));

Managing inline storage size
============================
By default, ``pw::Function`` stores its callable inline within the object. The
inline storage size defaults to the size of one pointer, but is configurable
through the build system.

:cpp:type:`pw::InlineFunction` is similar to ``pw::Function``,
but is always inlined. That is, even if dynamic allocation is enabled for
``pw::Function``, ``pw::InlineFunction`` will fail to compile if
the callable is larger than the inline storage size.

Attempting to construct a function from a callable larger than its inline size
is a compile-time error unless dynamic allocation is enabled.

.. admonition:: Inline storage size

   The default inline size of one pointer is sufficient to store most common
   callable objects, including function pointers, simple non-capturing and
   capturing lambdas, and lightweight custom classes.

.. code-block:: c++

   // The lambda is moved into the function's internal storage.
   pw::Function<int(int, int)> subtract([](int a, int b) { return a - b; });

   // Functions can be also be constructed from custom classes that implement
   // operator(). This particular object is large (8 ints of space).
   class MyCallable {
    public:
     int operator()(int value);

    private:
     int data_[8];
   };

   // Compiler error: sizeof(MyCallable) exceeds function's inline storage size.
   pw::Function<int(int)> function((MyCallable()));

Dynamic allocation
==================
You can configure the inline allocation size of ``pw::Function`` and whether it
dynamically allocates, but it applies to all uses of ``pw::Function``.

As mentioned in :ref:`module-pw_function-design`, ``pw::Function`` is an alias
of Fuchsia's ``fit::function``. ``fit::function`` allows you to specify the
inline (static) allocation size and whether to dynamically allocate if the
callable target doesn't inline. If you want to use a function class with
different attributes, you can interact with ``fit::function`` directly but note
that the resulting functions may not be interchangeable, i.e. callables for one
might not fit in the other.

When ``PW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION`` is enabled, a ``pw::Function``
will use dynamic allocation to store callables that exceed the inline size.
An :ref:`allocator <module-pw_allocator>` type can be optionally supplied as a
template argument. The default allocator type can also be changed by overriding
``PW_FUNCTION_DEFAULT_ALLOCATOR_TYPE`` (the ``value_type`` of the allocator
is irrelevant, since it must support rebinding). When dynamic allocation is
enabled but a compile-time check for the inlining is still required,
``pw::InlineFunction`` can be used.

.. warning::

   If ``PW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION`` is enabled then attempts to
   cast from :cpp:type:`pw::InlineFunction` to a regular
   :cpp:type:`pw::Function` will **ALWAYS** allocate memory.

Invoking ``pw::Function`` from a C-style API
============================================
.. _trampoline layers: https://en.wikipedia.org/wiki/Trampoline_(computing)

One use case for invoking ``pw_function`` from a C-style API is to automate the
generation of `trampoline layers`_. See
:cpp:type:`pw::function::GetFunctionPointer()`.

.. _module-pw_function-reference:

-------------
API reference
-------------

``pw::Function``
================
.. doxygentypedef:: pw::Function

``pw::InlineFunction``
======================
.. doxygentypedef:: pw::InlineFunction

``pw::Callback``
================
.. doxygentypedef:: pw::Callback

``pw::InlineCallback``
======================
.. doxygentypedef:: pw::InlineCallback

``pw::bind_member()``
=====================
.. doxygenfunction:: pw::bind_member

``pw::function::GetFunctionPointer()``
======================================
.. doxygenfile:: pw_function/pointer.h
   :sections: detaileddescription
.. doxygenfunction:: GetFunctionPointer()
.. doxygenfunction:: GetFunctionPointer(const FunctionType&)

``pw::function::GetFunctionPointerContextFirst()``
==================================================
.. doxygenfunction:: GetFunctionPointerContextFirst()
.. doxygenfunction:: GetFunctionPointerContextFirst(const FunctionType&)

``pw::ScopeGuard``
==================
.. doxygenclass:: pw::ScopeGuard
   :members:

.. _module-pw_function-design:

------
Design
------
``pw::Function`` is an alias of Fuchsia's ``fit::function_impl`` and
``pw::Callback`` is an alias of Fuchsia's ``fit::callback_impl``. See the
following links for more information about Fuchsia's implementations:

* `//third_party/fuchsia/repo/sdk/lib/fit/include/lib/fit/function.h <https://cs.opensource.google/pigweed/pigweed/+/main:third_party/fuchsia/repo/sdk/lib/fit/include/lib/fit/function.h>`_
* `fit::function <https://fuchsia.googlesource.com/fuchsia/+/HEAD/sdk/lib/fit/#fit_function>`_

.. _module-pw_function-non-literal:

Why ``pw::Function`` is not a literal
=====================================
The default constructor for ``pw::Function`` is ``constexpr`` but
``pw::Function`` is not a literal type. Instances can be declared ``constinit``
but can't be used in ``constexpr`` contexts. There are a few reasons for this:

* ``pw::Function`` supports wrapping any callable type, and the wrapped type
  might not be a literal type.
* ``pw::Function`` stores inline callables in a bytes array, which is not
  ``constexpr``-friendly.
* ``pw::Function`` optionally uses dynamic allocation, which doesn't work in
  ``constexpr`` contexts (at least before C++20).

------------
Size reports
------------

Comparing ``pw::Function`` to a traditional function pointer
============================================================
The following size report compares an API using a :cpp:type:`pw::Function` to a
traditional function pointer.

.. include:: function_size

Typical sizes of various callable types
=======================================
The table below demonstrates typical sizes of various callable types, which can
be used as a reference when sizing external buffers for ``pw::Function``
objects.

.. include:: callable_size


