.. _docs-blog-07-bazelcon-2024.rst:

==============================
Pigweed Blog #7: BazelCon 2024
==============================
*Published on 2024-12-13 by Ted Pudlik*

Pigweed core team members attended `BazelCon 2024
<https://events.linuxfoundation.org/bazelcon/>`__. We had a great time, learned
a lot, connected with the broader Bazel community—and gave two talks about our
work on Bazel and Pigweed!

------------------------------
Creating C++ toolchains easily
------------------------------
By Armando Montanez (Pigweed) and Matt Stark (Google).

Video
=====
.. raw:: html

   <iframe width="560" height="315"
           src="https://www.youtube.com/embed/PVFU5kFyr8Y"
           title="Creating C++ toolchains easily" frameborder="0"
           referrerpolicy="strict-origin-when-cross-origin" allowfullscreen>
   </iframe>

Abstract
========
Dive into the new `rules_cc`_ mechanism for defining C++ toolchains.  The new
mechanism (which we contributed) is simpler, safer, more explicit, and more
modular than the previous mechanism. It also removes confusing concepts such as
``compile_files``.

.. _rules_cc: https://github.com/bazelbuild/rules_cc

Further reading
===============
See :ref:`docs-blog-06-better-cpp-toolchains` for more about this work!

------------------------------------------
Pigweed and Bazel for embedded development
------------------------------------------
By Ted Pudlik (Pigweed).

Video
=====
.. raw:: html

   <iframe width="560" height="315"
           src="https://www.youtube.com/embed/eiHTMU6a7uQ"
           title="Pigweed and Bazel for embedded development" frameborder="0"
           referrerpolicy="strict-origin-when-cross-origin" allowfullscreen>
   </iframe>

Abstract
========
Pigweed is an open-source collection of embedded libraries. We aim to make
Bazel the best build system for developing microcontroller firmware. In this
talk, I'll discuss how Pigweed leverages Bazel's build configuration APIs and
platforms to solve common problems in building embedded firmware: building
binaries for multiple cores in one Bazel invocation, compiling flasher scripts
that embed the firmware, creating libraries with platform-specific
implementations, and more!
