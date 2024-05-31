.. _docs-blog-02-bazel-feature-flags:

==================================================
Pigweed Eng Blog #2: Feature flags in Bazel builds
==================================================
By Ted Pudlik

Published 2024-05-31

Let's say you're migrating your build system to Bazel. Your project heavily
relies on preprocessor defines to configure its code.

.. code-block::

   -DBUILD_FEATURE_CPU_PROFILE
   -DBUILD_FEATURE_HEAP_PROFILE
   -DBUILD_FEATURE_HW_SHA256

In your source files, you use these preprocessor variables to conditionally
compile some sections, via ``#ifdef``. When building the same code for
different final product configurations, you want to set different defines.

How do you model this in Bazel?

This post discusses three possible approaches:

#. :ref:`docs-blog-02-config-with-copts`
#. :ref:`docs-blog-02-platform-based-skylib-flags`
#. :ref:`docs-blog-02-chromium-pattern`

Which one to choose? If you have the freedom to refactor your code to use the
Chromium pattern, give it a try! It is not difficult to maintain once
implemented, and can prevent real production issues by detecting typos.

.. _docs-blog-02-config-with-copts:

-------------------------------------------
Easy but limited: bazelrc config with copts
-------------------------------------------
Let's start with the simplest approach: you can put the compiler options into
your `bazelrc configuration file <https://bazel.build/run/bazelrc>`_.

.. code-block::

   # .bazelrc

   common:mydevice_evt1 --copts=-DBUILD_FEATURE_CPU_PROFILE
   common:mydevice_evt1 --copts=-DBUILD_FEATURE_HEAP_PROFILE
   common:mydevice_evt1 --copts=-DBUILD_FEATURE_HW_SHA256
   # and so on

Then, when you build your application, the defines will all be applied:


.. code-block:: sh

   bazel build --config=mydevice_evt1 //src:application


Configs are expanded recursively, allowing you to group options together and
reuse them:

.. code-block::

   # .bazelrc

   common:full_profile --copts=-DBUILD_FEATURE_CPU_PROFILE
   common:full_profile --copts=-DBUILD_FEATURE_HEAP_PROFILE

   # When building for mydevice_evt1, use full_profile.
   common:mydevice_evt1 --config=full_profile

   # When building for mydevice_evt2, additionally enable HW_SHA256.
   common:mydevice_evt2 --config=full_profile
   common:mydevice_evt1 --copts=-DBUILD_FEATURE_HW_SHA256

Downsides
=========
While it *is* simple, the config-with-copts approach has a few downsides.

.. _docs-blog-02-config-dangeous-typos:

Dangerous typos
---------------
If you misspell ``BUILDDD_FEATURE_CPU_PROFILE`` [sic!] in your ``.bazelrc``,
the actual ``BUILD_FEATURE_CPU_PROFILE`` variable will `take the default value
of 0 <https://stackoverflow.com/q/5085392/24291280>`__. So, although you
intended to enable this feature, it will just remain disabled!

This isn't just a Bazel problem, but a general issue with the simple
``BUILD_FEATURE`` macro pattern. If you misspell ``BUILD_FEATUER_CPU_PROFILE``
[sic!] in your C++ file, you'll get it to evaluate to 0 in any build system!

One way to avoid this issue is to use the :ref:`"Chromium-style" build flag
pattern <docs-blog-02-chromium-pattern>`, ``BUILDFLAG(CPU_PROFILE)``. If you
do, a misspelled or missing define becomes a compiler error. However, the
config-with-copts approach is a little *too* simple to express this pattern,
which requires code generation of the build flag headers.

No multi-platform build support
-------------------------------
Bazel allows you to perform multi-platform builds. For example, in a single
Bazel invocation you can build a Python flasher program (that will run on your
laptop) which embeds as a data dependency the microcontroller firmware to flash
(that will run on the microcontroller). `We do this in Pigweed's own
examples
<https://cs.opensource.google/pigweed/examples/+/main:examples/01_blinky/BUILD.bazel>`__.

Unfortunately, the config-with-copts pattern doesn't work nicely with
multi-platform build primitives. (Technically, the problem is that Bazel
`doesn't support transitioning on --config
<https://bazel.build/extending/config#unsupported-native-options>`__.) If you
want a multi-platform build, you need some more sophisticated idiom.

No build system variables
-------------------------
This approach doesn't introduce any variable that can be used within the build
system to e.g. conditionally select different source files for a library,
choose a different library as a dependency, or remove some targets from the
build altogether. We're really just setting preprocessor defines here.


Limited multirepo support
-------------------------
The ``.bazelrc`` files are not automatically inherited when another repo
depends on yours. They can be `imported
<https://bazel.build/run/bazelrc#imports>`__, but it's an all-or-nothing
affair.

.. _docs-blog-02-platform-based-skylib-flags:

------------------------------------------------------------
More power with no code changes: Platform-based Skylib flags
------------------------------------------------------------
Let's address some shortcomings of the approach above by representing the build
features as `Skylib flags
<https://github.com/bazelbuild/bazel-skylib/blob/main/docs/common_settings_doc.md>`__
and grouping them through `platform-based flags
<https://github.com/bazelbuild/proposals/blob/main/designs/2023-06-08-platform-based-flags.md>`__.
(Important note: this feature is `still under development
<https://github.com/bazelbuild/bazel/issues/19409>`__! See :ref:`the Appendix
<docs-blog-02-old-bazel>` for workarounds for older Bazel versions.)

The platform sets a bunch of flags:

.. code-block:: python

   # //platform/BUILD.bazel

   # The platform definition
   platform(
     name = "mydevice_evt1",
     flags = [
       "--//build/feature:cpu_profile=true",
       "--//build/feature:heap_profile=true",
       "--//build/feature:hw_sha256=true",
     ],
   )

The flags have corresponding code-generated C++ libraries:

.. code-block:: python

   # //build/feature/BUILD.bazel
   load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
   # I'll show one possible implementation of feature_cc_library later.
   load("//:feature_cc_library.bzl", "feature_cc_library")

   # This is a boolean flag, but there's support for int- and string-valued
   # flags, too.
   bool_flag(
       name = "cpu_profile",
       build_setting_default = False,
   )

   # This is a custom rule that generates a cc_library target that exposes
   # a header "cpu_profile.h", the contents of which are either,
   #
   # BUILD_FEATURE_CPU_PROFILE=1
   #
   # or,
   #
   # BUILD_FEATURE_CPU_PROFILE=0
   #
   # depending on the value of the cpu_profile bool_flag. This "code
   # generation" is so simple that it can actually be done in pure Starlark;
   # see below.
   feature_cc_library(
       name = "cpu_profile_cc",
       flag = ":cpu_profile",
   )

   # Analogous library that exposes the constant in Python.
   feature_py_library(
       name = "cpu_profile_py",
       flag = ":cpu_profile",
   )

   # And in Rust, why not?
   feature_rs_library(
       name = "cpu_profile_rs",
       flag = ":cpu_profile",
   )

   bool_flag(
       name = "heap_profile",
       build_setting_default = False,
   )

   feature_cc_library(
       name = "heap_profile_cc",
       flag = ":heap_profile",
   )

   bool_flag(
       name = "hw_sha256",
       build_setting_default = False,
   )

   feature_cc_library(
       name = "hw_sha256_cc",
       flag = ":hw_sha256",
   )

C++ libraries that want to access the variable needs to depend on the
``cpu_profile_cc`` (or ``heap_profile_cc``, ``hw_sha256_cc``) library.

Here's one possible implementation of ``feature_cc_library``:

.. code-block:: python

   # feature_cc_library.bzl
   load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

   def feature_cc_library(name, build_setting):
       hdrs_name = name + ".hdr"

       flag_header_file(
           name = hdrs_name,
           build_setting = build_setting,
       )

       native.cc_library(
           name = name,
           hdrs = [":" + hdrs_name],
       )

   def _impl(ctx):
       out = ctx.actions.declare_file(ctx.attr.build_setting.label.name + ".h")

       # Convert boolean flags to canonical integer values.
       value = ctx.attr.build_setting[BuildSettingInfo].value
       if type(value) == type(True):
           if value:
               value = 1
           else:
               value = 0

       ctx.actions.write(
           output = out,
           content = r"""
   #pragma once
   #define {}={}
   """.format(ctx.attr.build_setting.label.name.upper(), value),
       )
       return [DefaultInfo(files = depset([out]))]

   flag_header_file = rule(
       implementation = _impl,
       attrs = {
           "build_setting": attr.label(
               doc = "Build setting (flag) to construct the header from.",
               mandatory = True,
           ),
       },
   )

Advantages
==========

Composability of platforms
--------------------------
A neat feature of the simple config-based approach was that configs could be
composed through recursive expansion. Fortunately, platforms can be composed,
too! There are two mechanisms for doing so:

#. Use platforms' `support for inheritance
   <https://bazel.build/reference/be/platforms-and-toolchains#platform_inheritance>`__.
   This allows "subplatforms" to override entries from "superplatforms". But,
   only single inheritance is supported (each platform has at most one parent).

#. The other approach is to compose lists of flags directly, through concatenation:

   .. code-block:: python

      FEATURES_CORTEX_M7 = [
        "--//build/feature:some_feature",
      ]

      FEATURES_MYDEVICE_EVT1 = FEATURES_CORTEX_M7 + [
        "--//build/feature:some_other_feature",
      ]

      platform(
        name = "mydevice_evt1",
        flags = FEATURES_MYDEVICE_EVT1,
      )

   Concatenation doesn't allow overriding entries, but frees you from the
   single-parent limitation of inheritance.

   .. tip::

      This approach can also be used to define custom host platforms:
      ``HOST_CONSTRAINTS`` in ``@local_config_platform//:constraints.bzl``
      contains the autodetected ``@platform//os`` and ``@platforms//cpu``
      constraints set by Bazel's default host platform.

Multi-platform build support
----------------------------
How do you actually associate the platform with a binary you want to build? One
approach is to just specify the platform on the command-line when building a
``cc_binary``:

.. code-block:: sh

   bazel build --platforms=//platform:mydevice_evt1 //src:main

But another approach is to leverage multi-platform build, through
`platform_data <https://github.com/bazelbuild/rules_platform/blob/main/platform_data/defs.bzl>`__:

.. code-block:: python

   # //src/BUILD.bazel
   load("@rules_platform//platform_data:defs.bzl", "platform_data")

   cc_binary(name = "main")

   platform_data(
       name = "main_mydevice_evt1",
       target = ":main",
       platform = "//platform:mydevice_evt1",
   )

Then you can keep your command-line simple:

.. code-block:: sh

   bazel build //src:main_mydevice_evt1



Flags correspond to build variables
-----------------------------------
You can make various features of the build conditional on the value of the
flag. For example, you can select different dependencies:

.. code-block:: python

   # //build/feature/BUILD.bazel
   config_setting(
     name = "hw_sha256=true",
     flag_values = {
       ":hw_sha256": "true",
     },
   )

   # //src/BUILD.bazel
   cc_library(
     name = "my_library",
     deps = [
       "//some/unconditional:dep",
     ] + select({
       "//build/feature:hw_sha256=true": ["//extra/dep/for/hw_sha256:only"],
       "//conditions:default": [],
   })

Any Bazel rule attribute described as `"configurable"
<https://bazel.build/docs/configurable-attributes>`__ can take a value that
depends on the flag in this way. Library header lists and source lists are
common examples, but the vast majority of attributes in Bazel are configurable.

Downsides
=========

Typos remain dangerous
----------------------
If you used :ref:`"Chromium-style" build flags <docs-blog-02-chromium-pattern>`
you *would* be immune to dangerous typos when using this Bazel pattern. But
until then, you still have this problem, and actually it got worse!

If you forget to ``#include "build/features/hw_sha256.h"`` in the C++ file that
references the preprocessor variable, the build system or compiler will still
not yell at you. Instead, the ``BUILD_FEATURE_HA_SHA256`` variable will take
the default value of 0.

This is similar to the :ref:`typo problem with the config approach
<docs-blog-02-config-dangeous-typos>`, but worse, because it's easier to miss
an ``#include`` than to misspell a name, and you'll need to add these
``#include`` statements in many places.

One way to mitigate this problem is to make the individual
``feature_cc_library`` targets private, and gather them into one big library
that all targets will depend on:

.. code-block:: python

   feature_cc_library(
       name = "cpu_profile_cc",
       flag = ":cpu_profile",
       visibility = ["//visibility:private"],
   )

   feature_cc_library(
       name = "heap_profile_cc",
       flag = ":heap_profile",
       visibility = ["//visibility:private"],
   )

   feature_cc_library(
       name = "hw_sha256_cc",
       flag = ":hw_sha256",
       visibility = ["//visibility:private"],
   )

   # Code-generated cc_library that #includes all the individual
   # feature_cc_library headers.
   all_features_cc_library(
       name = "all_features",
       deps = [
           ":cpu_profile_cc",
           ":heap_profile_cc",
           ":hw_sha256_cc",
           # ... and many more.
       ],
       visibility = ["//visibility:public"],
   )

However, a more satisfactory solution is to adopt :ref:`Chromium-style build
flags <docs-blog-02-chromium-pattern>`, which we discuss next.

Build settings have mandatory default values
--------------------------------------------
The Skylib ``bool_flag`` that represents the build flag within Bazel has a
``build_setting_default`` attribute. This attribute is mandatory.

This may be a disappointment if you were hoping to provide no default, and have
Bazel return errors if no value is explicitly set for a flag (either via a
platform, through ``.bazelrc``, or on the command line). The Skylib build flags
don't support this.

The danger here is that the default value may be unsafe, and you forget to
override it when adding a new platform (or for some existing platform, when
adding a new flag).

There is an alternative pattern that allows you to define default-less build
flags: instead of representing build flags as Skylib flags, you can represent
them as ``constraint_setting`` objects. I won't spell this pattern out in
this blog post, but it comes with its own drawbacks:

*  The custom code-generation rules are more complex, and need to parse the
   ``constraint_value`` names to infer the build flag values.
*  All supported flag values must be explicitly enumerated in the ``BUILD``
   files, and the code-generation rules need explicit dependencies on them.
   This leads to substantially more verbose ``BUILD`` files.

On the whole, I'd recommend sticking with the Skylib flags!


.. _docs-blog-02-chromium-pattern:

------------------------------------------------------------
Error-preventing approach: Chromium-style build flag pattern
------------------------------------------------------------
This pattern builds on :ref:`docs-blog-02-platform-based-skylib-flags` by
adding a macro helper for retrieving flag values that guards against typos. The
``BUILD.bazel`` files look exactly the same as in the :ref:`previous section
<docs-blog-02-platform-based-skylib-flags>`, but:

#. Users of flags access them in C++ files via ``BUILDFLAG(SOME_NAME)``.
#. The code generated by ``feature_cc_library`` is a little more elaborate than
   a plain ``SOME_NAME=1`` or ``SOME_NAME=0``, and it includes a dependency on
   the `Chromium build flag header
   <https://chromium.googlesource.com/chromium/src/build/+/refs/heads/main/buildflag.h>`__.

Here's the ``feature_cc_library`` implementation:

.. code-block:: python

   load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

   def feature_cc_library(name, build_setting):
       """Generates a cc_library from a common build setting.

       The generated cc_library exposes a header [build_setting.name].h that
       defines a corresponding build flag.

       Example:

           feature_cc_library(
               name = "evt1_cc",
               build_setting = ":evt1",
           )

       *  This target is a cc_library that exposes a header you can include via
          #include "build/flags/evt1.h".
       *  That header defines a build flag you can access in your code through
          BUILDFLAGS(EVT1).
       *  If you wish to use the build flag from a cc_library, add the target
          evt1_cc to your cc_library's deps.

       Args:
         name: Name for the generated cc_library.
         build_setting: One of the Skylib "common settings": bool_flag, int_flag,
           string_flag, etc. See
           https://github.com/bazelbuild/bazel-skylib/blob/main/docs/common_settings_doc.md
       """
       hdrs_name = name + ".hdr"

       flag_header_file(
           name = hdrs_name,
           build_setting = build_setting,
       )

       native.cc_library(
           name = name,
           hdrs = [":" + hdrs_name],
           # //:buildflag is a cc_library containing the
           # Chromium build flag header.
           deps = ["//:buildflag"],
       )

   def _impl(ctx):
       out = ctx.actions.declare_file(ctx.attr.build_setting.label.name + ".h")

       # Convert boolean flags to canonical integer values.
       value = ctx.attr.build_setting[BuildSettingInfo].value
       if type(value) == type(True):
           if value:
               value = 1
           else:
               value = 0

       ctx.actions.write(
           output = out,
           content = r"""
   #pragma once

   #include "buildflag.h"

   #define BUILDFLAG_INTERNAL_{}() ({})
   """.format(ctx.attr.build_setting.label.name.upper(), value),
       )
       return [DefaultInfo(files = depset([out]))]

   flag_header_file = rule(
       implementation = _impl,
       attrs = {
           "build_setting": attr.label(
               doc = "Build setting (flag) to construct the header from.",
               mandatory = True,
           ),
       },
   )

-----------
Bottom line
-----------
If you have the freedom to refactor your code to use the Chromium pattern,
Bazel provides safe and convenient idioms for expressing configuration through
build flags. Give it a try!

Otherwise, you can still use platform-based Skylib flags, but beware typos and
missing ``#include`` statements!

--------
Appendix
--------
A couple "deep in the weeds" questions came up while this blog post was being
reviewed. I thought they were interesting enough to discuss here, for the
interested reader!

Why isn't the reference code a library?
=======================================
If you made it this far you might be wondering, why is the code listing for
``feature_cc_library`` even here? Why isn't it just part of Pigweed, and used
in our own codebase?

The short answer is that Pigweed is middleware supporting multiple build
systems, so we don't want to rely on the build system to generate configuration
headers.

But the longer answer has to do with how this blog post came about. Some time
ago, I was migrating team A's build from CMake to Bazel. They used Chromium
build flags, but in CMake, so to do a build migration they needed Bazel support
for this pattern. So I put an implementation together. I wrote a design
document, but it had confidential details and was not widely shared.

Then team B comes along and says, "we tried migrating to Bazel but couldn't
figure out how to support build flags" (not the Chromium flags, but the "naive"
kind; i.e. their problem statement was exactly the one the blog opens with). So
I wrote a less-confidential but still internal doc for them saying "here's how
you could do it"; basically, :ref:`docs-blog-02-platform-based-skylib-flags`.

Then Pigweed's TL comes along and says "Ted, don't you feel like spending a day
fighting with RST [the markup we use for pigweed.dev]?" Sorry, actually they
said something more like, "Why is this doc internal, can't we share this more
widely"? Well we can. So that's the doc you're reading now!

But arguably the story shouldn't end here: Pigweed should probably provide a
ready-made implementation of Chromium build flags for downstream projects. See
`issue #342454993 <https://pwbug.dev/342454993>`_ to check out how that's
going!

Do you need to generate actual files?
=====================================
If you are a Bazel expert, you may ask: do we need to have Bazel write out the
actual header files, and wrap those in a ``cc_library``? If we're already
writing a custom rule for ``feature_cc_library``, can we just set ``-D``
defines by providing `CcInfo
<https://bazel.build/rules/lib/providers/CcInfo>`__? That is, do something like
this:

.. code-block:: python

   define = "{}={}"..format(
     ctx.attr.build_setting.label.name.upper(),
     value)
   return [CcInfo(
     compilation_context=cc_common.create_compilation_context(
       defines=depset([define])))]

The honest answer is that this didn't occur to me! But one reason to prefer
writing out the header files is that this approach generalizes in an obvious
way to other programming languages: if you want to generate Python or Golang
constants, you can use the same pattern, just change the contents of the file.
Generalizing the ``CcInfo`` approach is trickier!

.. _docs-blog-02-old-bazel:

What can I do on older Bazel versions?
======================================
This blog focused on describing approaches that rely on `platform-based
flags
<https://github.com/bazelbuild/proposals/blob/main/designs/2023-06-08-platform-based-flags.md>`__.
But this feature is very new: in fact, as of this writing, it is `still under
development <https://github.com/bazelbuild/bazel/issues/19409>`__, so it's not
available in *any* Bazel version! So what can you do?

One approach is to define custom wrapper rules for your ``cc_binary`` targets
that use a `transition
<https://bazel.build/extending/config#user-defined-transitions>`__ to set the
flags. You can see examples of such transitions in the `Pigweed examples
project
<https://cs.opensource.google/pigweed/examples/+/main:targets/transition.bzl>`__.
