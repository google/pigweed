.. _module-pw_function:

-----------
pw_function
-----------
The function module provides a standard, general-purpose API for wrapping
callable objects.

.. note::
  This module is under construction and its API is not complete.

Overview
========

Basic usage
-----------
``pw_function`` defines the ``pw::Function`` class. A ``Function`` is a
move-only callable wrapper constructable from any callable object. Functions
are templated on the signature of the callable they store.

Functions implement the call operator --- invoking the object will forward to
the stored callable.

.. code-block:: c++

  int Add(int a, int b) { return a + b; }

  // Construct a Function object from a function pointer.
  pw::Function<int(int, int)> add_function(Add);

  // Invoke the function object.
  int result = add_function(3, 5);
  EXPECT_EQ(result, 8);

  // Construct a function from a lambda.
  pw::Function<int(int)> negate([](int value) { return -value; });
  EXPECT_EQ(negate(27), -27);

Functions are nullable. Invoking a null function triggers a runtime assert.

.. code-block:: c++

  // A function intialized without a callable is implicitly null.
  pw::Function<void()> null_function;

  // Null functions may also be explicitly created or set.
  pw::Function<void()> explicit_null_function(nullptr);

  pw::Function<void()> function([]() {});  // Valid (non-null) function.
  function = nullptr;  // Set to null, clearing the stored callable.

  // Functions are comparable to nullptr.
  if (function != nullptr) {
    function();
  }

``pw::Function``'s default constructor is ``constexpr``, so default-constructed
functions may be used in classes with ``constexpr`` constructors and in
``constinit`` expressions.

.. code-block:: c++

  class MyClass {
   public:
    // Default construction of a pw::Function is constexpr.
    constexpr MyClass() { ... }

    pw::Function<void(int)> my_function;
  };

  // pw::Function and classes that use it may be constant initialized.
  constinit MyClass instance;

Storage
-------
By default, a ``Function`` stores its callable inline within the object. The
inline storage size defaults to the size of two pointers, but is configurable
through the build system. The size of a ``Function`` object is equivalent to its
inline storage size.

Attempting to construct a function from a callable larger than its inline size
is a compile-time error.

.. admonition:: Inline storage size

  The default inline size of two pointers is sufficient to store most common
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

..
  For larger callables, a ``Function`` can be constructed with an external buffer
  in which the callable should be stored. The user must ensure that the lifetime
  of the buffer exceeds that of the function object.

  .. code-block:: c++

    // Initialize a function with an external 16-byte buffer in which to store its
    // callable. The callable will be stored in the buffer regardless of whether
    // it fits inline.
    pw::FunctionStorage<16> storage;
    pw::Function<int()> get_random_number([]() { return 4; }, storage);

  .. admonition:: External storage

    Functions which use external storage still take up the configured inline
    storage size, which should be accounted for when storing function objects.

In the future, ``pw::Function`` may support dynamic allocation of callable
storage using the system allocator. This operation will always be explicit.

API usage
=========

Implementation-side
-------------------
When implementing an API which takes a callback, a ``Function`` can be used in
place of a function pointer or equivalent callable.

.. code-block:: c++

  // Before:
  void DoTheThing(int arg, void (*callback)(int result));

  // After. Note that it is possible to have parameter names within the function
  // signature template for clarity.
  void DoTheThing(int arg, pw::Function<void(int result)> callback);

An API can accept a function either by value or by reference. If taken by value,
the implementation is responsible for managing the function by moving it into an
appropriate location.

.. admonition:: Value or reference?

  It is preferable for APIs to take functions by value rather than by reference.
  This provides callers of the API with a more convenient interface, as well as
  making their lives easier by not requiring management of resources or
  lifetimes.

Caller-side
-----------
When calling an API which takes a function by reference, the standard pattern is
to implicitly construct the function in place from a callable object. Simply
pass the desired callable directly to the API.

.. code-block:: c++

  // Implicitly initialize a Function from a capturing lambda.
  DoTheThing(42, [this](int result) { result_ = result; });

Size reports
============

Function class
--------------
The following size report compares an API using a ``pw::Function`` to a
traditional function pointer.

.. include:: function_size

Callable sizes
--------------
The table below demonstrates typical sizes of various callable types, which can
be used as a reference when sizing external buffers for ``Function`` objects.

.. include:: callable_size

Design
======
``pw::Function`` is based largely on
`fbl::Function <https://cs.opensource.google/fuchsia/fuchsia/+/main:zircon/system/ulib/fbl/include/fbl/function.h>`_
from Fuchsia with some changes to make it more suitable for embedded
development.

Functions are movable, but not copyable. This allows them to store and manage
callables without having to perform bookkeeping such as reference counting, and
avoids any reliance on dynamic memory management. The result is a simpler
implementation which is easy to conceptualize and use in an embedded context.
