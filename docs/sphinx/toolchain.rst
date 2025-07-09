.. _toolchain:

=================
Pigweed Toolchain
=================
Pigweed Toolchain is an LLVM/Clang toolchain for embedded use cases.

.. _toolchain-background:

----------
Background
----------
Our toolchain philosophy is to co-evolve the support for embedded use cases in
the LLVM project as we onboard new projects to use the Clang/LLVM toolchain
distributed by us. Teams at Google that we partner with make contributions to the
LLVM project regularly. Recent contributions include:

* Compiler optimizations
* LLVM ``libc`` library support
* Improvements to the LLD linker

.. _live at HEAD: https://abseil.io/about/philosophy#we-recommend-that-you-choose-to-live-at-head

We follow the `live at HEAD`_ model and do all development in LLVM which enables
us to rapidly adopt new features and provide fast feedback to the LLVM community.
We continuously build new versions of the Clang toolchain binaries and distribute
them to our users. Pigweed's toolchain version is periodically updated along with
code changes necessary to the Pigweed codebase itself to be compatible with and
take advantage of new toolchain features as soon as they land.

For more information, check out the following presentation:

.. raw:: html

   <iframe width="560" height="315"
       src="https://www.youtube.com/embed/0HvgvBUPTyw"
       title="LLVM Toolchain for Embedded Systems"
       frameborder="0"
       allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share"
       referrerpolicy="strict-origin-when-cross-origin"
       allowfullscreen></iframe>

.. _toolchain-prebuilts:

---------
Prebuilts
---------
.. _CIPD Packages: https://chrome-infra-packages.appspot.com/p/fuchsia/third_party/clang

The toolchain can be downloaded from `CIPD Packages`_. It includes
a cross-compiler that can target the following architectures.

Embedded targets:

* ARM 32-bit

  * ARMv6

  * ARMv7

  * ARMv8

* RISC-V 32-bit

Host targets:

* Linux
* Windows\ :sup:`1`
* macOS\ :sup:`2`

:sup:`1` Contains some limitations around user-side licensing.

:sup:`2` MLInliner is not supported yet.

.. _toolchain-support:

-------
Support
-------
`File a bug <https://pwbug.dev>`_ or talk to the Pigweed
team on `Discord <https://discord.com/invite/M9NSeTA>`_.
