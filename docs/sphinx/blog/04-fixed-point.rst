.. _blog-04-fixed-point:

========================================================================
Pigweed Blog #4: Fixed-point arithmetic as a replacement for soft floats
========================================================================
*Published on 2024-09-03 by Leonard Chan*

Fixed-point arithmetic is an alternative to floating-point arithmetic for
representing fractional data values (0.5, -1.125, 10.75, etc.). Fixed-point
types can be represented as :ref:`scaled integers <blog-04-fixed-point-intro>`.
The advantage here is many
arithmetic operations (+, -, \*, /, etc.) can be implemented as normal integral
instructions. This can be useful for embedded systems or other applications
where floating-point operations can be too expensive or not available.
Clang has support for fixed-point types according to `ISO TR 18037 <https://standards.iso.org/ittf/PubliclyAvailableStandards/c051126_ISO_IEC_TR_18037_2008.zip>`_.
I work on a Google project that uses Pigweed.
Recently, we replaced usage of floats in an embedded project running
on Cortex M0+. This MCU doesn't provide any hardware support for floating-point
operations, so floating-point operations are implemented in software. We’ve
observed a **~2x speed improvement** in
classification algorithms and a **small decrease in binary size** using fixed-point
**without sacrificing correctness**.

All Clang users can benefit from this by adding ``-ffixed-point`` as a
compilation flag. All of the types and semantics described
in ISO 18037 are supported.

Problems with (soft) floats
===========================
One of the ways to represent a decimal number is to use a significand, base,
and exponent. This is how floating-point numbers according to IEEE 754 are
represented. The base is two, but the significand and exponent are adjustable.

When doing arithmetic with floats, many operations are needed to align the
exponents of the operands and normalize the result. Most modern computers have
Floating-Point Units (`FPUs <https://en.wikipedia.org/wiki/Floating-point_unit>`_) that do this arithmetic very quickly, but not all
platforms (especially embedded ones) have this luxury. The alternative is to
emulate floating-point arithmetic in software which can be costly.

.. _blog-04-fixed-point-intro:

----------------------------------------------
Intro to fixed-point types and scaled integers
----------------------------------------------
`ISO TR 18037 <https://standards.iso.org/ittf/PubliclyAvailableStandards/c051126_ISO_IEC_TR_18037_2008.zip>`_
describes 12 fixed-point types with varying scales and ranges:

* ``unsigned short _Fract``
* ``unsigned _Fract``
* ``unsigned long _Fract``
* ``signed short _Fract``
* ``signed _Fract``
* ``signed long _Fract``
* ``unsigned short _Accum``
* ``unsigned _Accum``
* ``unsigned long _Accum``
* ``signed short _Accum``
* ``signed _Accum``
* ``signed long _Accum``

The scale represents the number of fractional bits in the type. Under the hood,
Clang implements each of these types as integers. To get the decimal value of
the type, we just divide the integer by the scale. For example, on x86_64
Linux, an ``unsigned _Accum`` type is represented as an unsigned 32-bit integer
with a scale of 16, so 16 bits are used to represent the fractional part and
the remaining 16 are used to represent the integral part. An ``unsigned _Accum``
type with a value of ``16.625`` would be represented as ``1089536`` because
``1089536 / 2**16 = 16.625``. Additionally, an ``unsigned _Accum`` type has a range
of ``[0, 65535.99998474121]``, where the max value is represented as ``2^32-1``.
The smallest possible value we can represent with a fixed-point type is
``1/2^scale``.

The width and scale of each type are platform-dependent. In Clang, different
targets are free to provide whatever widths
and scales are best-suited to their needs and capabilities. Likewise,
LLVM backends are free to lower the different fixed-point intrinsics to native
fixed-point operations. The default configuration for all targets is the
“typical desktop processor” implementation described in A.3 of ISO TR 18037.

Each of these types has a saturating equivalent also (denoted by the ``_Sat``
keyword). This means operations on a saturating type that would normally cause
overflow instead clamps to the maximal or minimal value for that type.

Fixed-point to the rescue
=========================
One of the main advantages of fixed-point types is their operations can be done
with normal integer operations. Addition and subtraction between the same fixed-point
types normally require just a regular integral ``add`` or ``sub`` instruction.
Multiplication and division can normally be done with a regular ``mul`` and ``div``
instruction (plus an extra shift to account for the scale). Platforms that
don’t support hard floats normally need to make a call to a library function
that implements the floating-point equivalents of these which can be large or
CPU-intensive.

Fixed-point types however are limited in their range and precision which are
dependent on the number of integral and fractional bits. A ``signed _Accum``
(which uses 15 fractional bits, 16 integral bits, and 1 sign bit on x86_64
Linux) will have a range of ``[-65536, 65535.99996948242]`` and a precision of
``3.0517578125e-05``. Floats can effectively range from ``(-∞, +∞)`` and have
precision as small as :math:`10^{-38}`. Maintaining correctness for your program
largely depends on how much range and precision you intend your fractional
numbers to have. For addressing ranges, there are sometimes a couple of clever
math tricks you can do to get large values to fit within these ranges. Some of
these methods will be discussed later.

.. list-table:: Fixed-Point vs Floating-Point
   :widths: 10 45 45
   :header-rows: 1
   :stub-columns: 1

   * -
     - Fixed-Point
     - Floating-Point
   * - Range
     - Limited by integral and fractional bits
     - +/-∞
   * - Precision
     - Limited by the fractional bits (:math:`1/2^{scale}`)
     - :math:`10^{-38}`
   * - Binary Operations
     - | Typically fast
       | Usually just involves normal integral binary ops with some shifts
     - | *Hard floats* - Typically fast
       | *Soft floats* - Can be very slow/complex; depends on the implementation
   * - Correctness
     - | Always within 1 ULP of the true result
       | Will always produce the same result for a given op
     - | Always within 0.5 ULP of true result
       | May not always produce the same result for a given op due to rounding errors

---------------------
Running on Cortex-M0+
---------------------
The Arm Cortex-M0+ processor is a common choice for constrained embedded
applications because it is very energy-efficient. Because it doesn’t have an
FPU, many floating-point operations targeting this CPU depend on a helper
`library <https://github.com/ARM-software/abi-aa/blob/7c2fbbd6d32c906c709804f873b67308d1ab46e2/rtabi32/rtabi32.rst#the-standard-compiler-helper-function-library>`_
to define these as runtime functions. For our application, the code
running on this processor runs different classification algorithms. One such
classifier utilizes `softmax regression <http://ufldl.stanford.edu/tutorial/supervised/SoftmaxRegression/>`_.
The algorithm is roughly:

.. code-block:: c++

   // This is roughly a trimmed down version of the softmax regression
   // classifier.
   size_t Classify(std::span<const float> sample) const {
     // 1. Compute activations for each category.
     //    All activations are initially zero.
     std::array<float, NumCategories> activations{};
     for (size_t i = 0; i < NumCategories; i++) {
       for (size_t j = 0; j < sample.size(); j++) {
         activations[i] += coefficients_[i][j] * sample[j];
       }
       activations[i] += biases_[i];
     }
     float max_activation =
         *std::max_element(activations.begin(), activations.end());

     // 2. Get e raised to each of these activations.
     std::array<T, NumCategories> exp_activations;
     for (size_t i = 0; i < NumCategories; i++) {
       exp_activations[i] = expf(activations[i]);
     }
     float sum_exp_activations =
         std::accumulate(exp_activations.begin(), exp_activations.end(), 0);

     // 3. Compute each likelihood which us exp(x[i]) / sum(x).
     float max_likelihood = std::numeric_limits<float>::min();
     size_t prediction;
     for (size_t i = 0; i < NumCategories; i++) {
       // 0 <= result.likelihoods[i] < 1
       result.likelihoods[i] = exp_activations[i] / sum_exp_activations;
       if (result.likelihoods[i] > max_likelihood) {
         max_likelihood = result.likelihoods[i];
         prediction = i;  // The highest likelihood is the prediction.
       }
     }

     return prediction;  // Return the index of the highest likelihood.
   }

Many of these operations involve normal floating-point comparison, addition,
multiplication, and division. Each of these expands to a call to some
``__aeabi_*`` function. This particular function is on a hot path that
activates, meaning soft float operations take up much computation and power.
For the normal binary operations, fixed-point types might be a good fit because
it’s very likely each of the binary operations will result in a value that
can fit into one of the fixed-point types. (Each of
the sample values is in the range ``[-256, +256]`` and there are at most 10
categories. Likewise, each of the ``coefficients_`` and ``biases_`` are small values
ranging from roughly ``(-3, +3)``.)

If we wanted to completely replace floats for this function with fixed-point
types, we’d run into two issues:

#. There doesn’t exist an ``expf`` function that operates on fixed-point types.
#. ``expf(x)`` can grow very large for even small for small values of x
   (``expf(23)`` would exceed the largest value possible for an
   ``unsigned long _Accum``).

Fortunately, we can refactor the code as needed and we can make changes to
llvm-libc, the libc implementation this device uses.

llvm-libc and some math
=======================
The embedded device's codebase is dependent on a handful of functions that
take floats: ``expf``, ``sqrtf``, and ``roundf``.
``printf`` is also used for printing floats, but support for the fixed-point format
specifiers is needed. The project already uses llvm-libc, so we can implement
versions of these functions that operate on fixed-point types.
The llvm-libc team at Google has been very responsive
and eager to implement these functions for us! Michael Jones (who implemented
much of printf in llvm-libc) provided `printf support for each of the fixed
point types <https://github.com/llvm/llvm-project/pull/82707>`_. Tue Ly
provided implementations for `abs <https://github.com/llvm/llvm-project/pull/81823>`_,
`roundf <https://github.com/llvm/llvm-project/pull/81994>`_,
`sqrtf <https://github.com/llvm/llvm-project/pull/83042>`_,
`integral sqrt with a fixed-point output <http://github.com/llvm/llvm-project/issues/83924>`_,
and `expf <https://github.com/llvm/llvm-project/pull/84391>`_ for different
fixed-point types. Now we have the necessary math functions which accept
fixed-point types.

With implementations provided, the next big step is avoiding overflow and
getting results to fit within the new types. Let’s look at one case involving
``expf`` and one involving ``sqrtf``. (Tue Ly also provided these solutions.)

The softmax algorithm above easily causes overflow with:

.. code-block::

   exp_activations[i] = expf(activations[i]);

But the larger goal is to get the likelihood that a sample matches each
category. This is a value from [0, 1].

.. math::

   likelihood(i) = \frac{e^{activations[i]}}{\sum_{k=0}^{NumCategories}{e^{activation[k]}}} = \frac{e^{activation[i]]}}{sum(e^{activation[...]}))}

This can however be normalized by the max activation:

.. math::

   likelihood(i) = \frac{e^{activation[i]]}}{sum(e^{activation[...]}))} = \frac{e^{activation[i]]}}{sum(e^{activation[...]}))} * \frac{max(e^{activation[...]})}{max(e^{activation[...]})} = \frac{e^{activation[i]-max(activation[...])]}}{sum(e^{activation[...]-max(activation[...])}))}

This makes the exponent for each component at most zero and the result of
``exp(x)`` at most 1 which can definitely fit in the fixed-point types.
Likewise, the sum of each of these is at most ``NumCategories`` (which happens
to be 10 for us). For the above code, the only necessary change required is:

.. code-block::

   exp_activations[i] = expf(activations[i] - max_activation);

And lucky for us, the precision for a ``signed _Accum`` type is enough to get
us the same results. One interesting thing is this change alone also improved
the performance when using floats by 11%. The theory is the ``expf`` implementation
has a quick switch to see if the exponents need to be adjusted for floats, and
skips that scaling part when unnecessary.

The code involving ``sqrtf`` can be adjusted similarly:

.. code-block:: c++

   void TouchProcessor::CharacteriseRadialDeviation(Touch& touch) {
     // Compute center of touch.
     int32_t sum_x_w = 0, sum_y_w = 0, sum_w = 0;
     // touch.num_position_estimates is at most 100
     for (size_t i = 0; i < touch.num_position_estimates; i++) {
       sum_x_w += touch.position_estimates[i].position.x * 255;
       sum_y_w += touch.position_estimates[i].position.y * 255;
       sum_w += 255;
     }

     // Cast is safe, since average can't exceed any of the individual values.
     touch.center = Point{
         .x = static_cast<int16_t>(sum_x_w / sum_w),
         .y = static_cast<int16_t>(sum_y_w / sum_w),
     };

     // Compute radial deviation from center.
     float sum_d_squared_w = 0.0f;
     for (size_t i = 0; i < touch.num_position_estimates; i++) {
       int32_t dx = touch.position_estimates[i].position.x - touch.center.x;
       int32_t dy = touch.position_estimates[i].position.y - touch.center.y;
       sum_d_squared_w += static_cast<float>(dx * dx + dy * dy) * 255;
     }

     // deviation = sqrt(sum((dx^2 + dy^2) * w) / sum(w))
     touch.features[static_cast<size_t>(Touch::Feature::kRadialDeviation)] =
         sqrtf(sum_d_squared_w / sum_w);
   }

We know beforehand the maximal values of ``dx`` and ``dy`` are +/-750, so
``sum_d_squared_w`` will require as much as 35 integral bits to represent the
final sum. ``sum_w`` also needs 11 bits, so ``sqrtf`` would need to accept a value
that can be held in ~24 bits. This can definitely fit into a
``signed long _Accum`` which has 32 integral bits, but ideally we’d like to
use a ``_Sat signed _Accum`` for consistency. Similar to before, we can
normalize the value by 255. That is, we can avoid multiplying by 255 when
calculating ``sum_x_w``, ``sum_y_w``, and ``sum_w`` since this will result in
the exact same ``touch.center`` results. Likewise, if we avoid the ``* 255`` when
calculating ``sum_d_squared_w``, the final ``sqrtf`` result will be unchanged.
This makes the code:

.. code-block:: c++

   void TouchProcessor::CharacteriseRadialDeviation(Touch& touch) {
     // Compute center of touch.
     int32_t sum_x_w = 0, sum_y_w = 0, sum_w = touch.num_position_estimates;
     // touch.num_position_estimates is at most 100
     for (size_t i = 0; i < touch.num_position_estimates; i++) {
       sum_x_w += touch.position_estimates[i].position.x;
       sum_y_w += touch.position_estimates[i].position.y;
     }

     // Cast is safe, since average can't exceed any of the individual values.
     touch.center = Point{
         .x = static_cast<int16_t>(sum_x_w / sum_w),
         .y = static_cast<int16_t>(sum_y_w / sum_w),
     };

     // Compute radial deviation from center.
     float sum_d_squared_w = 0.0f;
     for (size_t i = 0; i < touch.num_position_estimates; i++) {
       int32_t dx = touch.position_estimates[i].position.x - touch.center.x;
       int32_t dy = touch.position_estimates[i].position.y - touch.center.y;
       sum_d_squared_w += static_cast<float>(dx * dx + dy * dy);
     }

     // deviation = sqrt(sum((dx^2 + dy^2) * w) / sum(w))
     touch.features[static_cast<size_t>(Touch::Feature::kRadialDeviation)] =
         sqrtf(sum_d_squared_w / sum_w);
   }

This allows ``sum_d_squared_w`` to fit within 31 integral bits. We can go
further and realize ``sum_d_squared_w`` will always have an integral value and
replace its type with a ``uint32_t``. This will mean any fractional part from
``sum_d_squared_w / sum_w`` will be discarded, but for our use case, this is
acceptable since the integral part of ``sum_d_squared_w / sum_w`` is so large
that the fractional part is negligible. ``touch.features[i]`` also isn’t
propagated significantly so we don’t need to worry about propagation of
error. With this, we can replace the ``sqrtf`` with one that accepts an integral
type and returns a fixed-point type:

.. code-block:: c++

   touch.features[static_cast<size_t>(Touch::Feature::kRadialDeviation)] =
       isqrt(sum_d_squared_w / sum_w);

-------
Results
-------

Classification runtime performance
==================================
Before any of this effort, a single run of classifying sensor input took on
average **856.8 us**.

Currently, with all floats substituted with fixed-point types, a single run is
**412.845673 us** which is a **~2.1x speed improvement**.

It’s worth noting that the optimizations we described above also improved
performance when using floats, making a single run using floats
**771.475464 us** which translates to **~1.9x speed improvement**.

Size
====
We’ve observed **~1.25 KiB** saved (out of a **~177 KiB** binary) when
switching to fixed-point.

.. code-block::

   $ bloaty app.elf.fixed -- app.elf.float
       FILE SIZE        VM SIZE
    --------------  --------------
     +0.5% +3.06Ki  [ = ]       0    .debug_line
     +0.1% +1.89Ki  [ = ]       0    .debug_str
     +0.2%    +496  [ = ]       0    .debug_abbrev
     +0.6%    +272  +0.6%    +272    .bss
     -0.3%     -16  -0.3%     -16    .data
     -0.3%    -232  [ = ]       0    .debug_frame
     -1.3%    -712  [ = ]       0    .debug_ranges
     -0.3% -1.11Ki  [ = ]       0    .debug_loc
     -1.2% -1.50Ki  -1.2% -1.50Ki    .text
     -1.6% -1.61Ki  [ = ]       0    .symtab
     -3.2% -3.06Ki  [ = ]       0    .pw_tokenizer.entries
     -1.2% -4.00Ki  [ = ]       0    .strtab
     -0.4% -19.6Ki  [ = ]       0    .debug_info
     -0.3% -26.1Ki  -0.7% -1.25Ki    TOTAL

A large amount of this can be attributed to not needing many of the
``__aeabi_*`` float functions provided by libc. All instances of floats were
removed so they don’t get linked into the final binary. Instead, we use the
fixed-point equivalents provided by llvm-libc. Much of these fixed-point
functions are also smaller and make fewer calls to other functions. For
example, the ``sqrtf`` we used makes calls to ``__ieee754_sqrtf``,
``__aeabi_fcmpun``, ``__aeabi_fcmplt``, ``__errno``, and ``__aeabi_fdiv``
whereas ``isqrtf_fast`` only makes calls to ``__clzsi2``, ``__aeabi_lmul``, and
``__aeabi_uidiv``. It’s worth noting that individual fixed-point binary
operations are slightly larger since they involve a bunch of shifts or
saturation checks. Floats tend to make a call to the same library function, but
fixed-point types emit instructions at every callsite.

.. code-block::

   float_add:
     push    {r7, lr}
     add     r7, sp, #0
     bl      __aeabi_fadd
     pop     {r7, pc}

   fixed_add:
     adds    r0, r0, r1
     bvc     .LBB1_2
     asrs    r1, r0, #31
     movs    r0, #1
     lsls    r0, r0, #31
     eors    r0, r1
   .LBB1_2:
     bx      lr

Although the callsite is shorter, it’s worth noting that the soft float
functions can’t be inlined because they tend to be very large. The
``__aeabi_fadd`` in particular is **444 bytes large**.

Correctness
===========
We observe the exact same results for our classification algorithms when
using fixed-point types. We have **over 15000 classification tests** with none
of them producing differing results. This effectively means we get all the
mentioned benefits without any correctness cost.

-----------------
Using fixed-point
-----------------
Fixed-point arithmetic can be used out-of-the-box with Clang by adding
``-ffixed-point`` on the command line. All of the types and semantics described
in ISO 18037 are supported. ``llvm-libc`` is the only ``libc`` that supports
fixed-point printing and math functions at the moment. Not all libc functions
described in ISO 18037 are supported at the moment, but they will be added
eventually! Pigweed users can use these fixed-point functions by using
the ``//pw_libc:stdfix`` target.

-----------
Future work
-----------
* An option we could take to reduce size (at the cost of even more performance)
  is to potentially teach LLVM to outline fixed-point additions. This could result
  in something similar to the float libcalls.
* See if we can instead use a regular ``signed _Accum`` in the codebase instead
  of a ``_Sat signed _Accum`` to save some instructions (or even see if we can
  use a smaller type).
* Investigate performance compared to hard floats. If we’re able to afford
  using an unsaturated type, then many native floating-point ops could be
  replaced with normal integral ops.
* Full libc support for the fixed-point C functions.
