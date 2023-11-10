.. _seed-0113:

===========================================
0113: Add modular Bazel C/C++ toolchain API
===========================================
.. seed::
   :number: 0113
   :name: Add modular Bazel C/C++ toolchain API
   :status: Accepted
   :proposal_date: 2023-09-28
   :cl: 173453

-------
Summary
-------
This SEED proposes custom Starlark rules for declaring C/C++ toolchains in
Bazel, and scalable patterns for sharing modular components of C/C++ toolchain
definitions.

----------
Motivation
----------
There is a lot of boilerplate involved in standing up a Bazel C/C++ toolchain.
While a good portion of the verbosity of specifying a new toolchain is
important, necessary machinery, nearly as much suffers from one or more of the
following problems:

- **Underdocumented patterns**: The
  `create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_
  method has an attribute called ``target_cpu`` that doesn't have an associated
  list of expected values, which suggests the argument is for bookkeeping
  purposes and doesn't affect Bazel behavior. The reality is this argument
  *does* have expected values that change behavior, but these are undocumented
  (see `this GitHub issue <https://github.com/bazelbuild/bazel/issues/19353>`_
  for more information).

- **Not inherently modular**: Bazel expects the overwhelming majority of a
  C/C++ toolchain to be specified as part of a call to
  ``create_cc_toolchain_config_info()``. Because this is a Starlark method,
  there's a lot of flexibility with how you construct a toolchain config, but
  very little by way of existing patterns for creating something that is
  testable, sharable, or in other ways modular. The existing
  `tutorial for creating a C/C++ toolchain <https://bazel.build/tutorials/ccp-toolchain-config#configuring_the_c_toolchain>`_
  illustrates expanding out the toolchain definition as a no-argument Starlark
  rule.

As Pigweed fully embraces multi-platform builds, it is critical for both
Pigweed and users of Pigweed that it is easy to stand up custom toolchain
definitions that work for the relevant hardware project-specific code/libraries.

This SEED seeks to address the shortcomings in Bazel C/C++ toolchain
declaration by establishing patterns and providing custom build rules that
are owned and maintained by Pigweed.

--------
Proposal
--------
1. Introduce rules for defining C/C++ toolchains
================================================
In an effort to improve the experience of defining a C/C++ toolchain, Pigweed
will introduce new Bazel rules that allow toolchains to share common boilerplate
without hindering power-user functionality.

While this work centers around an improved experience for defining a toolchain
via ``create_cc_toolchain_config_info()``, other build rules will be introduced
for closely related aspects of the toolchain definition process.

An example of what these rules would look like in practice is as follows:

.. code-block::

   # A tool that can be used by various build actions.
   pw_cc_tool(
       name = "clang_tool",
       path = "@cipd_llvm_toolchain//:bin/clang",
       additional_files = [
          "@cipd_llvm_toolchain//:all",
       ],
   )

   # A mapping of a tool to actions, with flag sets that define behaviors.
   pw_cc_action_config(
       name = "clang",
       actions = ALL_ASM_ACTIONS + ALL_C_COMPILER_ACTIONS,
       tools = [
           ":clang_tool",
       ],
       flag_sets = [
           # Most flags should NOT end up here. Only unconditional flags that
           # should ALWAYS be bound to this tool (e.g. static library
           # workaround fix for macOS).
           "@pw_toolchain//flag_sets:generate_depfile",
       ],
   )

   # A trivial flag to be consumed by a C/C++ toolchain.
   pw_cc_flag_set(
       name = "werror",
       actions = ALL_COMPILE_ACTIONS,
       flags = [
           "-Werror",
       ],
   )

   # A list of flags that can be added to a toolchain configuration.
   pw_cc_flag_set(
       name = "user_compile_options",
       actions = ALL_COMPILE_ACTIONS,
       flag_groups = [
           ":user_compile_options_flags",
       ]
   )

   # A more complex compiler flag that requires template expansion.
   pw_cc_flag_group(
       name = "user_compile_options_flags",
       flags = ["%{user_compile_flags}"],
       iterate_over = "user_compile_flags",
       expand_if_available = "user_compile_flags",
   )

   # The underlying definition of a complete C/C++ toolchain.
   pw_cc_toolchain(
       name = "host_toolchain_linux",
       action_configs = [
           ":clang",
           ":clang++",
           # ...
       ],
       additional_files = ":linux_sysroot_files",
       action_config_flag_sets = [
           "@pw_toolchain//flag_sets:no_canonical_prefixes",
           ":user_compile_options",
           ":werror",
       ],
       features = [
           "@pw_toolchain//features:c++17",
       ],
       target_cpu = "x86_64",
       target_system_name = "x86_64-unknown-linux-gnu",
       toolchain_identifier = "host-toolchain-linux",
   )

   # Toolchain resolution parameters for the above C/C++ toolchain.
   toolchain(
       name = "host_cc_toolchain_linux",
       exec_compatible_with = [
           "@platforms//os:linux",
       ],
       target_compatible_with = [
           "@platforms//os:linux",
       ],
       toolchain = ":host_toolchain_linux",
       toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
   )

2. Provide standard toolchain building-blocks
=============================================
Pigweed will build out a repository of sharable instantiations of the
aforementioned custom rules to give projects the resources they need to quickly
and easily assemble toolchains for desktop and embedded targets. This includes,
but is not limited to:

- Rules that define tool sets for common toolchains (LLVM/clang, GNU/gcc).
- Fully specified, modular
  `features <https://bazel.build/docs/cc-toolchain-config-reference#features>`_.
- Common flag sets that users may want to apply directly to their toolchains.
  (enabling/disabling warnings, C++ standard version, etc.)
- Platform/architecture support rules, including host OS SDK integrations
  (Xcode, Windows SDK) and architecture-specific flag sets.

These components will help establish patterns that will make it significantly
easier for Pigweed users (and Bazel users at large) to define their own
toolchains.

---------------------
Problem investigation
---------------------
This section explores previous work, and details why existing solutions don't
meet Pigweed's needs.

bazelembedded/rules_cc_toolchain
================================
The `rules_cc_toolchain <https://github.com/bazelembedded/rules_cc_toolchain>`_
as part of the larger bazelembedded suite was actually the initial foundation
of Pigweed's Bazel build. While this served as a very good initial foundation,
it didn't provide the flexibility needed to easily stand up additional
toolchains in ways that gave downstream projects sufficient control over the
flags, libraries, tools, and sysroot.

To work around the limited configurability of toolchain flags, Pigweed employed
the following workarounds:

#. Place ``copts`` and ``linkopts`` in ``.bazelrc``: This was problematic
   because ``.bazelrc`` is not intrinsically shared with or propagated to
   downstream users of Pigweed. Also, flags here are unilaterally applied
   without OS-specific considerations.
#. Attach flags to build targets with custom wrappers: This approach
   intrinsically requires the existence of the ``pw_cc_library``, which
   introduces difficulty around consistent interoperability with other Bazel
   projects (among other issues detailed in
   `b/267498492 <https://issues.pigweed.dev/issues/267498492>`_).

Some other issues encountered when working with this solution include:

- These rules intended to be modular, but in practice were relatively tightly
  coupled.
- Transitive dependencies throughout the toolchain definition process resulted
  in some hard-to-debug issues (see
  `this pull request <https://github.com/bazelembedded/rules_cc_toolchain/pull/39>`_
  and `b/254518544 <https://issues.pigweed.dev/issues/254518544>`_.

bazelembedded/modular_cc_toolchains
===================================
The `modular_cc_toolchains <https://github.com/bazelembedded/modular_cc_toolchains>`_
repository is a new attempt as part of the bazelembedded suite at providing
truly modular toolchain rules. The proposed direction is much more in-line
with the needs of Pigweed, but at the moment the repository exists as an
initial draft of ideas rather than a complete implementation.

This repository greatly inspired Pigweed's initial prototype for modular
toolchains, but diverges significantly from the underlying Bazel C/C++
toolchain building-blocks. If this work was already complete and
well-established, it probably would have satisfied some of Pigweed's key needs.

lowRISC/crt
===========
The `compiler repository toolkit <https://github.com/lowRISC/crt>`_ is another
scalable approach at toolchains. This repository strives to be an all-in-one
repository for embedded toolchains, and does a very good job at providing
scalable models for establishing toolchains. This repository is relatively
monolithic, though, and doesn't necessarily address the concern of quickly
and easily standing up custom toolchains. Instead, it's more suited towards
contributing new one-size-fits-all toolchains to ``crt`` directly.

Android's toolchain
===================
Android's Bazel-based build has invested heavily in toolchains, but they're
very tightly coupled to the use cases of Android. For example,
`this <https://cs.android.com/android/platform/superproject/main/+/main:build/bazel/toolchains/clang/host/linux-x86/cc_toolchain_features.bzl;l=375-389;drc=097d710c349758fc2732497fe5c3d1b0a32fa4a8>`_ binds ``-fstrict-aliasing`` to a condition based on the target architecture.
These toolchains scale for the purpose of Android, but unfortunately are
inherently not modular or reusable outside of that context.

Due to the sheer amount of investment in these toolchains, though, they serve
as a good reference for building out a complete toolchain in Bazel.

Pigweed's modular Bazel toolchain prototype
===========================================
As part of an exploratory phase of getting toolchains set up for Linux and
macOS,
`an initial prototype <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157634>`_
for modular Bazel toolchains was drafted and deployed to Pigweed. This work
introduced two key build rules: ``pw_cc_toolchain_feature`` and
``pw_cc_toolchain``. With both of these rules, it’s possible to instantiate a
vast array of toolchain variants without writing a single line of Starlark. A
few examples of these building blocks in action are provided below.

.. code-block::

   # pw_cc_toolchain example taken from https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain/host_clang/BUILD.bazel;l=113-143;drc=7df1768d915fe11dae05751f70f143e60acfb17a.

   pw_cc_toolchain(
       name = "host_toolchain_linux",
       abi_libc_version = "unknown",
       abi_version = "unknown",
       all_files = ":all_linux_files",
       ar = "@llvm_toolchain//:bin/llvm-ar",

       # TODO: b/305737273 - Globbing all files for every action has a
       # performance hit, make these more granular.
       ar_files = ":all_linux_files",
       as_files = ":all_linux_files",
       compiler = "unknown",
       compiler_files = ":all_linux_files",
       coverage_files = ":all_linux_files",
       cpp = "@llvm_toolchain//:bin/clang++",
       dwp_files = ":all_linux_files",
       feature_deps = [
           ":linux_sysroot",
            "@pw_toolchain//features:no_canonical_prefixes",
       ],
       gcc = "@llvm_toolchain//:bin/clang",
       gcov = "@llvm_toolchain//:bin/llvm-cov",
       host_system_name = "unknown",
       ld = "@llvm_toolchain//:bin/clang++",
       linker_files = ":all_linux_files",
       objcopy_files = ":all_linux_files",
       strip = "@llvm_toolchain//:bin/llvm-strip",
       strip_files = ":all_linux_files",
       supports_param_files = 0,
       target_cpu = "unknown",
       target_libc = "unknown",
       target_system_name = "unknown",
       toolchain_identifier = "host-toolchain-linux",
   )

   # pw_cc_toolchain_feature examples taken from https://cs.opensource.google/pigweed/pigweed/+/main:pw_toolchain_bazel/features/BUILD.bazel;l=21-34;drc=f96fd31675d136bd37a7f3840102cb256d555cea.

   # Disables linking of the default C++ standard library to allow linking of a
   # different version.
   pw_cc_toolchain_feature(
       name = "no_default_cpp_stdlib",
       linkopts = ["-nostdlib++"],
   )

   # Prevent relative paths from being converted to absolute paths.
   # Note: This initial prototype made this a feature, but it should instead
   # exist as a flag_set.
   pw_cc_toolchain_feature(
       name = "no_canonical_prefixes",
       copts = [
           "-no-canonical-prefixes",
       ],
   )

What’s worth noting is that the ``pw_cc_toolchain_feature`` build rule looks
very similar to a GN ``config``. This was no mistake, and was an attempt to
substantially reduce the boiler plate for creating new sharable compiler flag
groups.

Unfortunately, it quickly became apparent that this approach limited control
over the underlying toolchain definition creation process. In order to support
``always_link`` on macOS, a custom logic and flags had to be directly baked into
the rule used to declare toolchains
(`relevant change <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168614/17/pw_toolchain_bazel/cc_toolchain/private/cc_toolchain.bzl>`_).
While workarounds like this should be possible, the fact that this had to be
upstreamed internally to ``pw_cc_toolchain`` exposed limitations in the
abstraction patterns that were established. Such limitations could preclude
some project from using ``pw_cc_toolchain`` at all.

---------------
Detailed design
---------------
The core design proposal is to transform the providers used by
``cc_common.create_cc_toolchain_config_info()`` into build rules. The approach
has been prototyped
`here <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168351/1>`_,
and retains API compatibility with the initial prototype as a proof-of-concept.

One core pattern established by this design is transforming content that would
typically live as Starlark to instead live in build files. This is done to
make it easier to read and reference existing work.

Implementation requirements
===========================
Compatibility with native C/C++ rules
-------------------------------------
The core of Pigweed's toolchain build rules will rely on the providers
defined as part of Bazel's
`rules_cc <https://github.com/bazelbuild/rules_cc/blob/main/cc/cc_toolchain_config_lib.bzl>`_. This means that the new rules can interop with
existing work that directly uses these toolchain primitives. It also provides
a clear path for migrating existing toolchains piece-by-piece (which may be
written completely in Starlark).

Any extensions beyond the existing providers (e.g. specifying
``additional_files`` on a ``pw_cc_tool``) must happen parallel to existing
providers so that rules that consume the ``cc_toolchain_config_lib`` providers
can work with vanilla providers.

Compatibility with Bazel rules ecosystem
----------------------------------------
In following with the larger Bazel rules ecosystem, the toolchain building
blocks will be designed such that they can be used independently from Pigweed.
This allows this work to be used for non-embedded projects, and reduces the
overhead for standing up a custom Bazel C/C++ toolchain in any arbitrary
project.

Initially, the work will live as ``pw_toolchain_bazel`` in the main Pigweed
repository to facilitate testing. This module must not depend on any other
aspects of Pigweed. As the toolchain rules mature, they will eventually be
available as a separate repository to match the modularity patterns used by
the larger Bazel rules ecosystem.

Introduce ``pw_cc_flag_set`` and ``pw_cc_flag_group``
=====================================================
The majority of build flags would be expressed as ``pw_cc_flag_set`` and
``pw_cc_flag_group`` pairs.

.. code-block::

   # A simple flag_set with a single flag.
   pw_cc_flag_set(
       name = "werror",
       # Only applies to C/C++ compile actions (i.e. no assemble/link/ar).
       actions = ALL_CPP_COMPILER_ACTIONS + ALL_C_COMPILER_ACTIONS,
       flags = [
           "-Werror",
       ],
   )

   # A flag_group that potentially expands to multiple flags.
   pw_cc_flag_group(
       name = "user_compile_options_flags",
       flags = ["%{user_compile_flags}"],
       iterate_over = "user_compile_flags",
       expand_if_available = "user_compile_flags",
   )

   # A flag_set that relies on a non-trivial or non-constant expression of
   # flags.
   pw_cc_flag_set(
       name = "user_compile_options",
       actions = ALL_COMPILE_ACTIONS,
       flag_groups = [
           ":user_compile_options_flags",
       ]
   )

These closely mimic the API of ``cc_toolchain_config_lib.flag_set()`` and
``cc_toolchain_config_lib.flag_group()``, with the following exceptions:

**pw_cc_flag_set**

*Added*

- ``flags`` (added): Express a constant, trivial list of flags. If this is
  specified, ``flag_groups`` may not be specified. This eliminates the need
  for specifying a corresponding ``pw_cc_flag_group`` for every
  ``pw_cc_flag_set`` for most flags.

**pw_cc_flag_group**

*Removed*

- ``expand_if_true``\, ``expand_if_false``\, ``expand_if_equal``\: More complex
  rules that rely on these should live as custom Starlark rules that provide a
  ``FlagGroupInfo``\, or ``FlagSetInfo`` (depending on which is more ergonomic
  to express the intent). See :ref:`pw_cc_flag_set-exceptions` below for an
  example that illustrates how express more complex ``flag_group``\s that rely
  on these attributes.

Application of flags
--------------------
Flags can be applied to a toolchain in three ways. This section attempts to
provide initial guidance for where flags should be applied, though it's likely
better practices will evolve as this work sees more use. For the latest
guidance, please consult the official documentation when it rolls out to
``pw_toolchain_bazel``.

Flags unconditionally applied to a toolchain
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The majority of flags fall into this category. Architecture flags,
globally-applied warnings, global defines, and other similar flags should be
applied in the ``action_config_flag_sets`` attribute of a ``pw_cc_toolchain``
(see :ref:`pw_cc_toolchain-toolchain-declarations` for more information). Each
``pw_cc_flag_set`` (or other rule that provides a ``FlagSetInfo`` provider)
listed in ``action_config_flag_sets`` is unconditionally applied to every tool
that matches the ``actions`` listed in the flag set.

.. _application-of-flags-feature-flags:

Feature flags
~~~~~~~~~~~~~
Flag sets applied as features may or may not be enabled even if they are listed
in the ``features`` attribute of a ``pw_cc_toolchain``. The
`official Bazel documentation on features <https://bazel.build/docs/cc-toolchain-config-reference#features>`_
provides some good guidance on when features should be employed. To summarize,
features should be used when either they should be controllable by users
invoking the build, or if they affect build behavior beyond simply
adding/removing flags (e.g. by introducing additional build actions).

Flags unconditionally applied to a tool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
These flags are flags that are bound to a particular tool. These are not
expressed as part of a ``pw_cc_toolchain``, and are instead bound to a
``pw_cc_action_config``. This means that the flag set is unconditionally
applied to every user of that action config. These kinds of flag applications
should be reserved for flags required to assemble a working set of tools (such
as generating a depfile, or adding support for static library link handling
:ref:`as illustrated below <pw_cc_flag_set-exceptions>`).

Flag application order
~~~~~~~~~~~~~~~~~~~~~~
When invoking the underlying tools, the intended order of flags is as follows:

#. Flags listed in the ``flag_sets`` list of a ``pw_cc_action_config``.
#. Flags listed in ``action_config_flag_sets`` of a ``pw_cc_toolchain``.
#. Flags listed in ``features`` of a ``pw_cc_toolchain``.

These lists are intended to be sensitive to ordering, earlier items in the lists
should appear in the final tool invocation flags before later items in the list.

As transitive dependencies between features/flags are not supported as part of
this proposal, exact traversal of transitive flag dependencies will be left
to be decided if/when that feature is introduced. This proposal suggests
postorder handling of flags as the most intuitive order.

.. _pw_cc_flag_set-exceptions:

Exceptions
----------
Some flags are too complex to be nicely expressed in a Bazel build file. These
flag sets or flag groups will need to be expressed in Starlark as custom rules.
Fortunately, this will interop well with simpler flag sets since the underlying
providers are all the same.

**Example**

In a Starlark file (e.g. ``//tools/llvm/llvm_ar_patch.bzl``), the required
``flag_set`` can be defined:

.. code-block::

   # Starlark rules in a .bzl file for a relatively complicated workaround for
   # what would normally be inherently managed by Bazel internally.
   # TODO: b/297413805 - Remove this implementation.

   def _pw_cc_static_libs_to_link_impl():
      """Returns a flag_set provider that sets up static libraries to link."""
      return flag_set(
               actions = [
                   ACTION_NAMES.cpp_link_static_library,
               ],
               flag_groups = [
                   flag_group(
                       expand_if_available = "libraries_to_link",
                       iterate_over = "libraries_to_link",
                       flag_groups = [
                           flag_group(
                               expand_if_equal = variable_with_value(
                                   name = "libraries_to_link.type",
                                   value = "object_file",
                               ),
                               flags = ["%{libraries_to_link.name}"],
                           ),
                           flag_group(
                               expand_if_equal = variable_with_value(
                                   name = "libraries_to_link.type",
                                   value = "object_file_group",
                               ),
                               flags = ["%{libraries_to_link.object_files}"],
                               iterate_over = "libraries_to_link.object_files",
                           ),
                       ],
                   ),
               ],
           )

   pw_cc_static_libs_to_link = rule(
       implementation = _pw_cc_static_libs_to_link_impl,
       provides = [FlagSetInfo],
   )

And then in the ``BUILD.bazel`` file, the rules would be used as if they
were a ``pw_cc_flag_set``:

.. code-block::

   load(
       "@pw_toolchain//tools/llvm:llvm_ar_patch.bzl",
       "pw_cc_static_libs_to_link"
   )

   pw_cc_static_libs_to_link(
       name = "static_library_action_flags",
   )

   pw_cc_action_config(
       name = "llvm_ar",
       actions = ACTION_NAMES.cpp_link_static_library,
       tools = [
           ":llvm_ar_tool",
       ],
       flag_sets = [
           ":static_library_action_flags",
       ],
   )

Introduce ``pw_cc_feature`` and ``pw_cc_feature_set``
=====================================================
These types are just permutations of the ``cc_toolchain_config_lib.feature()``
and ``cc_toolchain_config_lib.with_feature_set()`` API. For guidance on when
these should be used, see
:ref:`application of feature flags <application-of-flags-feature-flags>`.

.. code-block::

   pw_cc_feature_set(
       name = "static_pie_requirements",
       with_features = ["pie"],
       # If this doesn't work when certain features are enabled, they should
       # be specified as ``without_features``.
   )

   pw_cc_feature(
       name = "static_pie",
       flag_sets = [
           "//flag_sets:static_pie",
       ],
       implies = ["static_link_flag"],
       requires = [
           ":static_pie_requirements",
       ],
   )

Introduce ``pw_cc_action_config`` and ``pw_cc_tool``
====================================================
These are closely related to the ``ActionConfigInfo`` and ``ToolInfo``
providers, but allow additional files to be attached and a list of actions to
be attached rather than a single action.

.. code-block::

   pw_cc_tool(
       name = "clang_tool",
       path = "@llvm_toolchain//:bin/clang",
       additional_files = [
           "@llvm_toolchain//:all",
       ],
   )

   pw_cc_action_config(
       name = "clang",
       actions = ALL_ASM_ACTIONS + ALL_C_COMPILER_ACTIONS,
       tools = [
           ":clang_tool",
       ],
       flag_sets = [
           # Most flags should NOT end up here. Only unconditional flags that
           # should ALWAYS be bound to this tool (e.g. static library
           # workaround fix for macOS).
           "//flag_sets:generate_depfile",
       ],
   )

.. _pw_cc_toolchain-toolchain-declarations:

Toolchain declarations
======================
In following with the other proposed rules, ``pw_cc_toolchain`` largely
follows the API of ``cc_common.create_cc_toolchain_config_info()``. Most of the
attributes are logically passed through, with the following exceptions:

- **action_config_flag_sets**: Flag sets to apply to action configs. Since flag
  sets are intrinsically bound to actions, there’s no need to divide them at
  this level.
- **additional_files**: Now that tools can spec out required files, those
  should be propagated and mostly managed internally. The ``\*_files`` members
  will still be available, but shouldn’t see much use. additional_files is like
  “all_files”, but applies to all action_configs.

.. code-block::

   pw_cc_toolchain(
       name = "host_toolchain_linux",
       abi_libc_version = "unknown",  # We should consider how to move this out in the future.
       abi_version = "unknown",
       action_configs = [
           "@llvm_toolchain//tools:clang",
           "@llvm_toolchain//tools:clang++",
           "@llvm_toolchain//tools:lld",
           "@llvm_toolchain//tools:llvm_ar",
           "@llvm_toolchain//tools:llvm_cov",
           "@llvm_toolchain//tools:llvm_strip",
       ],
       additional_files = ":linux_sysroot_files",
       action_config_flag_sets = [
           ":linux_sysroot",
           "@pw_toolchain//flag_collections:strict_warnings",
           "@pw_toolchain//flag_sets:no_canonical_prefixes",
       ],
       features = [
           "@pw_toolchain//features:c++17",
       ],
       host_system_name = "unknown",
       supports_param_files = 0,  # Seems like this should be attached to a pw_cc_action_config...
       target_cpu = "unknown",
       target_libc = "unknown",
       target_system_name = "unknown",
       toolchain_identifier = "host-toolchain-linux",
       cxx_builtin_include_directories = [
           "%package(@llvm_toolchain//)%/include/x86_64-unknown-linux-gnu/c++/v1",
           "%package(@llvm_toolchain//)%/include/c++/v1",
           "%package(@llvm_toolchain//)%/lib/clang/17/include",
           "%sysroot%/usr/local/include",
           "%sysroot%/usr/include/x86_64-linux-gnu",
           "%sysroot%/usr/include",
       ],
   )

------------
Alternatives
------------
Improve Bazel's native C/C++ toolchain rules
============================================
Improving Bazel's native rules for defining C/C++ toolchains is out of the
scope of Pigweed's work. Changing the underlying toolchain API as Bazel
understands it is a massive undertaking from the perspective of migrating
existing code. We hope that the custom rule effort can help guide future
decisions when it comes to toolchain scalability and maintainability.

----------
Next steps
----------
Rust toolchain interop
======================
Pigweed's Rust toolchains have some interoperability concerns and requirements.
The extend of this needs to be thoroughly investigated as a next step to ensure
that the Rust/C/C++ toolchain experience is relatively unified and ergonomic.

More maintainable ``cxx_builtin_include_directories``
=====================================================
In the future, it would be nice to have a more sharable solution for managing
``cxx_builtin_include_directories`` on a ``pw_cc_toolchain``. This could
plausibly be done by allowing ``pw_cc_flag_set`` to express
``cxx_builtin_include_directories`` so they can be propagated back up to the
``pw_cc_toolchain``.

Feature name collision guidance
===============================
Features support relatively complex relationships among each other, but
traditionally rely on string names to express these relationships rather than
labels. This introduces significant ambiguity, as it's possible for multiple
features to use the same logical name so long as they aren't both employed in
the same toolchain. In practice, the only way to tell what features will end up
enabled is to manually unpack what features a toolchain pulls in, and
cross-reference it against the output of
`--experimental_save_feature_state <https://bazel.build/reference/command-line-reference#flag--experimental_save_feature_state>`_.

One potential solution to this problem is to add a mechanism for expressing
features as labels, which will allow relationships to be expressed more
concretely, and help prevent unintended naming collisions. This would not
replace the ability to express relationships with features not accessible via
labels, but rather live alongside it.
