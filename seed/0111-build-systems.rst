.. _seed-0111:

===============================================
0111: Make Bazel Pigweed's Primary Build System
===============================================
.. seed::
   :number: 111
   :name: Make Bazel Pigweed's Primary Build System
   :status: Accepted
   :proposal_date: 2023-09-26
   :cl: 171695
   :authors: Ted Pudlik
   :facilitator: Armando Montanez

-------
Summary
-------
This SEED proposes that Pigweed transition to using `Bazel
<https://bazel.build/>`_ as its primary build system, replacing `GN
<https://gn.googlesource.com/gn/>`_ in that role.

Pigweed is and will continue to be a multi-build-system project. As modular
middleware, Pigweed aspires to be easy to integrate with existing embedded
projects whatever their build system. To facilitate this, we provide BUILD
files for multiple systems (Bazel, CMake, GN, Soong), as well as other
distributables where applicable (npm packages, Python wheels, etc).

But Pigweed is more than just a collection of modules. Pigweed offers a quick,
ergonomic way to start a new embedded project, as well as developer tooling
that lets it scale from a prototype to production deployment. And if you're
starting a new project using Pigweed from day one, you will ask: which build
system *should* I use? This is what we mean by Pigweed's *primary* build
system.

Pigweed's primary build system has been GN, but soon will be Bazel.

----------
Motivation
----------
GN has been Pigweed's primary build system since inception, and we've developed
an extensive GN build that was used to successfully ship products at scale. GN
is fast and extensible, and its flexible toolchain abstraction is well-suited
to the multi-target builds that arise in most embedded projects.

But GN has limitations:

#.  **Small community.** GN is a niche build system: the only major open-source
    projects that use it are Chromium and Fuchsia. It is not championed by any
    major nonprofit or corporation. Few users or open-source contributors come
    to Pigweed with any past experience with GN.
#.  **No reusable rulesets.** Pigweed has written and maintains all its GN
    rules: for C/C++, for Python, for Go (though those are deprecated). With
    Rust entering Pigweed, we are now developing GN rules for Rust. There are
    no built-in or community-provided rules we could adopt instead. Developing
    all rules in-house gives us flexibility, but requires large up-front and
    ongoing investments.
#.  **No hermetic builds.** GN offers no sandboxing and relies on timestamps to
    decide if outputs need to be rebuilt. This has undesirable consequences:

    *  The boundary between the environment produced by
       :ref:`module-pw_env_setup` and GN is blurred, making GN-built Pigweed as
       a whole hostile to systems like Docker or remote execution services.
    *  Incremental builds can become corrupted. Deleting the output directory
       and environment is an undesirable but necessary piece of every Pigweed
       developer's toolkit.
    *  Reliably running only affected tests in CQ is not possible.

We would like Pigweed to recommend a build system that does not suffer from these
limitations.

These limitations are not new. What's changed is the build system landscape.
When Pigweed was started years ago, GN was the best choice for a project
emphasizing multi-target builds. But the alternatives have now matured.

--------
Proposal
--------
The proposal is to make Bazel the recommended build system to use with Pigweed,
and the best overall build system for embedded developers. This will involve a
combination of contributions to Pigweed itself, to existing open-source Bazel
rules we wish to reuse, and when necessary to core Bazel.

The vision is not merely to achieve feature parity with Pigweed's GN offering
while addressing the limitations identified above, but to fully utilize the
capabilities provided by Bazel to produce the best possible developer
experience. For example, Bazel offers native support for external dependency
management and remote build execution. We will make it easy for Pigweed
projects to leverage features like these.

*  **What about GN?** Pigweed's GN support will continue, focusing on
   maintenance rather than new build features. No earlier than 2026, if no
   Pigweed projects are using GN, we may remove GN support. *The approval of
   this SEED does not imply approval of removing GN support.* This decision is
   explicitly deferred until a future date.

*  **What about CMake?** Because of its wide adoption in the C++ community,
   CMake will be supported indefinitely at the current level.

-------
Roadmap
-------
This section lists the high-level milestones for Pigweed's Bazel support, and
then dives into the specific work needed to reach them.

This roadmap is our plan of record as of the time of writing, but like all SEED
content it represents a snapshot in time. We are not as committed to the
specific dates as we are to the general direction.

There's no specific action that users must take by any date. But our
recommendations about build system choice (embodied in docs and in what we tell
people when they ask us) will change at some point.

Milestones
==========
*  **M0: Good for Most.** We can recommend Bazel as the build system for most
   new projects. We may not have full parity with GN yet, but we're close enough
   that the benefits of adopting Bazel exceed the costs, even in the short run.
   The target date for this milestone is the end of 2023.

   * Out of scope for M0: Windows support. We have to start somewhere, and we're
     starting with Linux and MacOS.

*  **M1: Good for All.** We can recommend Bazel for all new Pigweed projects,
   including ones that need Windows support.  The target date is end of Q1
   2024. After this date, we don't expect any new projects to use GN.

*  **M2: Best.** We develop compelling features for embedded within the
   Bazel ecosystem. This will happen throughout 2024.

Technical tracks
================
There are three main technical tracks:

*  **Configurable toolchains** exist for host and embedded, for C++ and Rust.
   A separate upcoming SEED will cover this area in detail, but the high-level
   goal is to make it straightforward to create families of related toolchains
   for embedded targets. This is required for milestone M0, except for Windows
   support, which is part of M1. The overall tracking issue is `b/300458513
   <https://issues.pigweed.dev/issues/300458513>`_.

*  **Core build patterns** (facades, multi-platform build, third-party crate
   deps for Rust) are established, documented, and usable.

   * M0:

     * Module configuration is supported in Bazel, `b/234872811
       <https://issues.pigweed.dev/issues/234872811>`_.
     * Bazel proto codegen is feature-complete, `b/301328390
       <https://issues.pigweed.dev/issues/301328390>`_.
     * Multiplatform build is ergonomic thanks to the adoption of
       `platform_data
       <https://github.com/bazelbuild/proposals/blob/main/designs/2023-06-08-standard-platform-transitions.md#depend-on-a-target-built-for-a-different-platform>`_
       and `platform-based flags
       <https://github.com/bazelbuild/proposals/blob/main/designs/2023-06-08-platform-based-flags.md>`_, `b/301334234
       <https://issues.pigweed.dev/issues/301334234>`_.
     * Clang sanitizers (asan, msan, tsan) are easy to enable in the Bazel build, `b/301487567
       <https://issues.pigweed.dev/issues/301487567>`_.

   * M1:

     * On-device testing pattern for Bazel projects developed and documented, `b/301332139
       <https://issues.pigweed.dev/issues/301332139>`_.
     * Sphinx documentation can be built with Bazel.
     * OSS Fuzz integration through Bazel.

*  **Bootstrap** for Bazel projects is excellent. This includes offering
   interfaces to Pigweed developer tooling like :ref:`module-pw_console`,
   :ref:`module-pw_cli`, etc.

   * M0: GN-free bootstrap for Bazel-based projects is designed and prototyped, `b/274658181
     <https://issues.pigweed.dev/issues/274658181>`_.

   * M1: Pigweed is straightforward to manage as a Bazel dependency, `b/301336229
     <https://issues.pigweed.dev/issues/301336229>`_.

*  **Onboarding** for users new to Pigweed-on-Bazel is easy thanks to
   excellent documentation, including examples.

   * M0:

     * There is a Bazel example project for Pigweed, `b/299994234
       <https://issues.pigweed.dev/issues/299994234>`_.
     * We have a "build system support matrix" that compares the features
       available in the three main build systems (Bazel, CMake, GN),
       `b/301481759 <https://issues.pigweed.dev/issues/301481759>`_.

   * M1:

     * The sample project has Bazel support, `b/302150820
       <https://issues.pigweed.dev/issues/302150820>`_.

------------
Alternatives
------------
The main alternatives to investing in Bazel are championing GN or switching to
a different build system.

Champion GN
===========
Pigweed does not have the resources to bring GN to parity with modern build
systems like Bazel, Buck2, or Meson. This is an area where we should partner
with another large project rather than build capabilities ourselves.

CMake
=====
CMake is `the most popular build system for C++ projects
<https://www.jetbrains.com/lp/devecosystem-2021/cpp/#Which-project-models-or-build-systems-do-you-regularly-use>`_,
by a significant margin. We already offer some CMake support in Pigweed. But
it's not a viable candidate for Pigweed's primary build system:

* **No multi-toolchain builds** Unlike Bazel and GN, CMake does not support
  multi-toolchain builds.
* **No Python or Rust support** Again unlike Bazel and GN, CMake is primarily
  focused on building C++ code. But Pigweed is a multilingual project, and
  Python and Rust need first-class treatment.
* **No hermetic builds** Unlike Bazel, CMake does not support sandboxing.

Many developers are attracted to CMake by its IDE support. Fortunately, `IDE
support for Bazel is also well-developed <https://bazel.build/install/ide>`_.

Other build systems
===================
There are other multi-lingual, correctness-emphasizing build systems out there,
most prominently `Meson <https://mesonbuild.com/>`_ and `Buck2
<https://buck2.build/>`_. We did not consider them realistic targets for
migration at this time. They offer similar features to Bazel, and we have an
existing Bazel build that's in use by some projects, as well as a closer
relationship with the Bazel community.

--------------
Open questions
--------------
Additional SEEDs related to Bazel support are anticipated but have not yet been
written. They will be linked from here once they exist.

* `SEED-0113
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173453>`_:
  Modular Bazel C/C++ toolchain API
* SEED-????: Pigweed CI/CQ for Bazel

----------------------------
Appendix: Why Bazel is great
----------------------------
This SEED has not focused on why Bazel is a great build system. This is because
we are not choosing Bazel over other major build systems, like Meson or Buck2,
for its specific features. We are motivated to recommend a new build system
because of GN's limitations, and we choose Bazel because we have a pre-existing
community of Bazel users, developers with Bazel experience, and a close
relationship with the Bazel core team.

But actually, Bazel *is* great! Here are some things we like best about it:

*  **Correct incremental builds.** It's great to be able to trust the build
   system to just do the right thing, including on a rebuild.
*  **External dependency management.** Bazel can manage external dependencies
   for you, including lazily downloading them only when needed. By leveraging
   this, we expect to speed up Pigweed bootstrap from several minutes to
   several seconds.
*  **Remote build execution** Bazel has excellent native support for `executing
   build actions in a distributed manner on workers in the cloud
   <https://bazel.build/remote/rbe>`_. Although embedded builds are typically
   small, build latency and infra test latency is a recurring concern among
   Pigweed users, and leveraging remote builds should allow us to dramatically
   improve performance in this area.
*  **Python environment management.** The Python rules for Bazel take care of
   standing up a Python interpreter with a project-specific virtual
   environment, a functionality we had to develop in-house for our GN build.
*  **Multilingual support.** Bazel comes with official or widely adopted
   third-party rules for C++, Python, Java, Go, Rust, and other langauges.
*  **Active community.** The Bazel Slack is always helpful, and GitHub issues
   tend to receive swift attention.
