.. _docs-embedded-cpp:

==================
Embedded C++ Guide
==================

This page contains recommendations for using C++ for embedded software. For
Pigweed code, these should be considered as requirements. For external
projects, these recommendations can serve as a resource for efficiently using
C++ in embedded projects.

These recommendations are subject to change as the C++ standard and compilers
evolve, and as the authors continue to gain more knowledge and experience in
this area. If you disagree with recommendations, please discuss them with the
Pigweed team, as we're always looking to improve the guide or correct any
inaccuracies.

Constexpr functions
===================
Constexpr functions are functions that may be called from a constant
expression, such as a template parameter, constexpr variable initialization, or
``static_assert`` statement. Labeling a function ``constexpr`` does not
guarantee that it is executed at compile time; if called from regular code, it
will be compiled as a regular function and executed at run time.

Constexpr functions are implicitly inline, which means they are suitable to be
defined in header files. Like any function in a header, the compiler is more
likely to inline it than other functions. Marking non-trivial functions as
``constexpr`` could increase code size, so check the compilation results if this
is a concern.

Simple constructors should be marked ``constexpr`` whenever possible. GCC
produces smaller code in some situations when the ``constexpr`` specifier is
present. Do not avoid important initialization in order to make the class
constexpr-constructible unless it actually needs to be used in a constant
expression.

Constexpr variables
===================
Constants should be marked ``constexpr`` whenever possible. Constexpr variables
can be used in any constant expression, such as a non-type template argument,
``static_assert`` statement, or another constexpr variable initialization.
Constexpr variables can be initialized at compile time with values calculated by
constexpr functions.

``constexpr`` implies ``const`` for variables, so there is no need to include
the ``const`` qualifier when declaring a constexpr variable.

Unlike constexpr functions, constexpr variables are **not** implicitly inline.
Constexpr variables in headers must be declared with the ``inline`` specifier.

.. code-block:: cpp

  namespace pw {

  inline constexpr const char* kStringConstant = "O_o";

  inline constexpr float kFloatConstant1 = CalculateFloatConstant(1);
  inline constexpr float kFloatConstant2 = CalculateFloatConstant(2);

  }  // namespace pw

Function templates
==================
Function templates facilitate writing code that works with different types. For
example, the following clamps a value within a minimum and maximum:

.. code-block:: cpp

  template <typename T>
  T Clamp(T min, T max, T value) {
    if (value < min) {
      return min;
    }
    if (value > max) {
      return min;
    }
    return value;
  }

The above code works seamlessly with values of any type -- float, int, or even a
custom type that supports the < and > operators.

The compiler implements templates by generating a separate version of the
function for each set of types it is instantiated with. This can increase code
size significantly.

.. tip::

  Be careful when instantiating non-trivial template functions with multiple
  types.

Virtual functions
=================
Virtual functions provide for runtime polymorphism. Unless runtime polymorphism
is required, virtual functions should be avoided. Virtual functions require a
virtual table, which increases RAM usage and requires extra instructions at each
call site. Virtual functions can also inhibit compiler optimizations, since the
compiler may not be able to tell which functions will actually be invoked. This
can prevent linker garbage collection, resulting in unused functions being
linked into a binary.

When runtime polymorphism is required, virtual functions should be considered.
C alternatives, such as a struct of function pointers, could be used instead,
but these approaches may offer no performance advantage while sacrificing
flexibility and ease of use.

.. tip::

  Only use virtual functions when runtime polymorphism is needed.
