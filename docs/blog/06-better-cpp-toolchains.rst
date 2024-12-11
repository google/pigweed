.. _docs-blog-06-better-cpp-toolchains:

=======================================================================
Pigweed Eng Blog #6: Shaping a better future for Bazel C/C++ toolchains
=======================================================================
*Published on 2024-12-11 by Armando Montanez*

----------------
Pigweed 💚 Bazel
----------------
Not too long ago, the Pigweed team announced our transition to a Bazel-first
ecosystem. You can read more about the decision in :ref:`seed-0111`, but the key
takeaway is that we're committed to making sure Bazel has a good story for
embedded software development. There's a surprising amount of history behind
this, and I thought it'd be helpful to unpack some of our journey.

This blog post is the first in a series on getting Bazel to work well for
embedded firmware projects, so keep an eye out for others that will come along.

----------------------------------------
First things first, we need a toolchain!
----------------------------------------
Pigweed traditionally prioritized our `GN <https://gn.googlesource.com/gn/>`__
build, and getting Bazel to work for embedded devices presented very different
challenges. One of the first major differences between Bazel and GN is the
experience of compiling a simple ``Hello, World!`` example. In Bazel, this "just
works" out of the box for most major operating systems. With GN, you can't start
to compile C/C++ code without first defining the details of your toolchain such
as which binaries to use, how to construct the arguments/flags passed to the
tools, and which specific flags to pass, so we were forced to learn those
intricacies right out the gate.

Problems with Bazel's default toolchain
=======================================
.. _hermetic: https://bazel.build/basics/hermeticity

For a long time, Pigweed relied on Bazel's default toolchain, primarily because
Pigweed's Bazel build mostly just existed for interoperability with Google’s
internal codebase. Bazel’s default C/C++ toolchain is not `hermetic`_, and its
behaviors are auto-generated in a way that is very unintuitive to inspect and
debug. Because we weren't defining our own toolchain, we regularly hit several
major issues:

* Builds used binutils/gcc rather than our preferred llvm/clang toolchain.
* CI builds and local builds were inconsistent; just because builds passed
  locally didn't mean the same build would pass in automated builds.
* All flags had to be passed in via the command line or pushed into a
  ``.bazelrc`` file.
* There was no clear path for scalably supporting embedded MCU device builds.

We knew at some point we'd have to configure our own toolchain, but we also knew
it wouldn’t be easy.

-------------------------
Why is it this difficult?
-------------------------
Bazel toolchains are quite powerful, and with great power comes great
complexity. Traditionally, a C/C++ toolchain in Bazel is expected to be declared
in `Starlark <https://bazel.build/rules/language>`__ rather than ``BUILD``
files, which makes them a little harder to find and read. Also, there’s
documentation fragmentation around Bazel’s toolchain API that can make it
difficult to understand how all the moving pieces of toolchains interact. For
years there was a distinct lack of good examples to learn from, too. Setting up
a simple, custom toolchain isn’t necessarily a lot of typing, but it’s
surprisingly difficult because of the sheer amount of Bazel implementation
details and behaviors you must first understand.

We looked into declaring a custom C++ toolchain in the early days of Pigweed’s
Bazel build, but quickly realized it would be quite a chore. What we didn’t
realize was that this would turn out to be a problem that would take much design
and discussion over the course of multiple years.

------------------
The journey begins
------------------
The early origins of this story are really thanks to Nathaniel Brough, who was
one of Pigweed’s first community contributors and an original pioneer of
Pigweed’s Bazel build. Nat put together
`a Bazel toolchain definition <https://github.com/bazelembedded/rules_cc_toolchain>`__
that Pigweed adopted and used until late 2023. This allowed us to build Pigweed
using a consistent version of clang, but wasn’t quite perfect. The initial
toolchain Nat put together got us off the ground, but came with a few
limitations:

* Configurability was limited. Pigweed didn’t have quite as much control over
  flags as we wanted.
* The declared toolchain configuration was quite rigid, so it was difficult to
  adjust or adapt to different sysroots or compilers.
* The most significant limitation, though, was that it wasn't configurable
  enough to scalably support targeting many different embedded MCUs in a flexible way.

Nat went back to the drawing board, and started drafting out
`modular_cc_toolchains <https://github.com/bazelembedded/modular_cc_toolchains>`__.
This proposal promised much more modular building blocks that would provide more
direct access to the underlying constructs exposed by Bazel for defining a
modular toolchain.

---------------------------------------------
Pigweed’s first attempt at modular toolchains
---------------------------------------------
In mid-2023 I was passed the ball for conclusively solving Pigweed’s Bazel C/C++
toolchain problems. I had previously spent a lot of time in Pigweed’s GN build
maintaining toolchain integration, so I was relatively familiar with what went
well and what didn’t. I dove into Bazel with a naive vision: make declaring a
toolchain as simple as listing a handful of tools and an assortment of ordered,
groupable flags:

.. code-block:: py

   pw_cc_toolchain(
      name = "host_toolchain",
      cc = "@llvm_clang//:bin/clang",
      cxx = "@llvm_clang//:bin/clang++",
      ld = "@llvm_clang//:bin/clang++",
      flags = [
         ":cpp_version",
         ":size_optimized",
      ]
   )

   pw_cc_flags(
      name = "cpp_version",
      copts = ["-std=c++17"],
   )

   pw_cc_flags(
      name = "size_optimized",
      copts = ["-Os"],
      linkopts = ["-Os"],
   )

This approach brought a few major improvements:

* Pigweed (and downstream projects) could now declare toolchains quite easily.
* We were able to make Bazel use the clang/llvm toolchain binaries that we host
  in `CIPD <https://chromium.googlesource.com/chromium/src/+/67.0.3396.27/docs/cipd.md>`__.

The big drawback with this approach was that it obscured a lot of Bazel’s
toolchain complexity in a way that limited Pigweed’s ability to cleanly and
modularly introduce fixes.

------------------
Making it official
------------------
With the learnings from my first attempt, I went back over Nat’s work for
`modular_cc_toolchains <https://github.com/bazelembedded/modular_cc_toolchains>`__,
and set out authoring :ref:`seed-0113`. There were some discussions on this API,
and the upstream Bazel owners of
`Bazel's C/C++ rules (rules_cc) <https://github.com/bazelbuild/rules_cc>`__
expressed interest in the work too. Eventually, the SEED was approved, and
landed largely as described. This toolchain API checked the critical boxes for
Pigweed: we could now declare toolchains in a scalable, modular way!

The only major remaining wart was handling of
`toolchain features <https://bazel.build/docs/cc-toolchain-config-reference#features>`__
and `action names <https://bazel.build/docs/cc-toolchain-config-reference#actions>`__\:
Pigweed didn’t try to innovate in this area as a first pass. The primary
reasoning behind this was we quickly learned that part of our advice would be to
recommend against a large proliferation of toolchain features. Still there was
room for improvement, especially since specifying action names on a flag set was
a little unwieldy:

.. code-block:: py

   load(
      "@pw_toolchain//cc_toolchain:defs.bzl",
      "pw_cc_flag_set",
      "ALL_CPP_COMPILER_ACTIONS",
      "ALL_C_COMPILER_ACTIONS",
   )

   pw_cc_flag_set(
      name = "werror",
      # These symbols have to be `load()`ed since they're string lists.
      actions = ALL_CPP_COMPILER_ACTIONS + ALL_C_COMPILER_ACTIONS,
      flags = [
         "-Werror",
      ],
   )

.. inclusive-language: disable

This work caught the eye of Matt Stark, who was interested in building out
toolchains for ChromeOS. He noticed these shortcomings, and put in a lot of work
to make these types type-safe by changing them to also be provided through build
rules:

.. inclusive-language: enable

.. code-block:: py

   load("@pw_toolchain//cc_toolchain:defs.bzl", "pw_cc_flag_set")

   pw_cc_flag_set(
      name = "werror",
      # Much nicer!
      actions = [
         "@pw_toolchain//actions:all_c_compiler_actions",
         "@pw_toolchain//actions:all_cpp_compiler_actions",
      ],
      flags = [
         "-Werror",
      ],
   )

.. todo-check: disable

There were a few other changes along the way to support these kinds of
expressions, but by and large the toolchain API has served Pigweed quite well;
it allowed us to finally close out many bugs strewn about the codebase that said
things along the lines of “TODO: someday this should live in a toolchain”. We
even used it when setting up initial Bazel support for the
`Raspberry Pi Pico SDK <https://github.com/raspberrypi/pico-sdk/>`__, and
the only changes required were
`a few tweaks to the toolchain template build files <http://pwrev.dev/194591>`__
to get it working on Windows for the first time.

.. todo-check: enable

.. inclusive-language: disable

------------------------
Making it SUPER official
------------------------
As Matt finished out his improvements to Pigweed’s toolchain rules, he posed the
question we’d previously considered: could this work just live in
`rules_cc <https://github.com/bazelbuild/rules_cc>`__, the source of truth for
Bazel’s C/C++ rules? We were optimistic, and reached out again to the owners.
The owners of rules_cc enthusiastically gave us the green light, and Matt took
Pigweed’s Bazel C/C++ toolchain constructs and began the process of upstreaming
the work to rules_cc. There have been some changes along the way (particularly
with naming), but they’ve been part of an effort to be more forward-looking
about guiding the future of the underlying constructs.

.. inclusive-language: enable

-----------
Try it out!
-----------
These rules were initially launched in
`rules_cc v0.0.10 <https://github.com/bazelbuild/rules_cc/releases/tag/0.0.10>`__,
and have since received a slew of updates, improvements, and most importantly
documentation/examples. Today, we’re ready to more broadly encourage projects to
try out the new work. We hope that these rules will become the preferred
foundations for declaring C/C++ toolchains in Bazel. We’re excited to see how
the wider Bazel community expands on these foundational building blocks!

If you’d like to give these rules a spin, check out the following resources and examples:

* `Creating C++ Toolchains Easily - Bazelcon Presentation by Matt Stark & Armando Montanez <https://www.youtube.com/watch?v=PVFU5kFyr8Y>`__
* `rules_cc toolchain API <https://github.com/bazelbuild/rules_cc/blob/main/docs/toolchain_api.md>`__
* `rules_cc living rule-based toolchain example <https://github.com/bazelbuild/rules_cc/tree/main/examples/rule_based_toolchain>`__
* `Raspberry Pi Pico SDK Bazel toolchain <https://github.com/raspberrypi/pico-sdk/blob/6587f5cc9a91ca7fef7ccf56420d465b88d8d398/bazel/toolchain/BUILD.bazel>`__
* `Pigweed’s host clang toolchain <https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain/host_clang/BUILD.bazel>`__

--------------
Special thanks
--------------
This work would not have been possible without Nat and Matt’s contributions. Nat
spent a lot of time collaborating with Pigweed to really kickstart the Bazel
effort, and Matt’s enthusiasm for finishing out Pigweed’s fledgling toolchain
API and getting it pushed into rules_cc quickly has been inspiring! A lot of
work went into solving this problem, and community contributions were a critical
part of the journey. Also, a very special thanks to Ivo List, who reviewed many
CLs as part of moving the toolchain rules into rules_cc.

This has been an amazing journey, and I'm excited for a better future for
C/C++ toolchains in Bazel!
