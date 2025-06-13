.. _module-pw_kernel:

=========
pw_kernel
=========
.. pigweed-module::
   :name: pw_kernel

.. note::

   This is an early draft. The content may change significantly over the
   next few months.

The Pigweed Kernel (``pw_kernel``) is an **experimental, Rust-based
kernel** for embedded systems. It prioritizes memory safety,
flexible security models, and a modern developer experience.

- **Rust foundation**: Leverages Rust's safety and concurrency features for
  a robust core.
- **Security-focused**: Offers adaptable security, from MPU/PMP-protected
  modes for process isolation to lightweight, no-protection modes for
  resource-constrained environments.
- **Scalable & interoperable**: Works across various system sizes
  with strong C++ and Rust compatibility.
- **Test-driven development**: Emphasizes comprehensive unit testing for
  all core primitives to ensure reliability.

.. warning::

   ``pw_kernel`` is currently experimental. APIs are subject to change.

.. admonition:: Who is this for?
   :class: tip

   ``pw_kernel`` is an exciting choice if you're looking to build on a modern,
   Rust-based RTOS and are comfortable working with experimental software.

   It's particularly well-suited for:

   - Projects exploring the benefits of Rust (memory safety, concurrency) in an
     embedded kernel context.
   - Developers interested in contributing to or influencing the direction of a
     new, security-conscious kernel.
   - Systems where C++ and Rust interoperability at the kernel-userspace boundary
     is a key requirement.
   - Teams looking for a kernel with a strong emphasis on testability and a path
     towards a verifiable TCB.

   However, as an experimental project, it is not yet suitable for:

   - Projects requiring a fully mature, production-hardened kernel with an
     extensive set of stable features and broad, long-term support guarantees.
   - Systems needing immediate access to a rich ecosystem of device drivers or
     advanced, fully-vetted memory protection schemes, as these are still under
     active development.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart
      :link: quickstart
      :link-type: doc
      :class-item: sales-pitch-cta-primary

      Build, test, and run ``pw_kernel`` examples in upstream Pigweed.

   .. grid-item-card:: :octicon:`list-unordered` Guides
      :link: guides
      :link-type: doc
      :class-item: sales-pitch-cta-secondary

      Learn about specific ``pw_kernel`` features and tools.

.. grid:: 2

   .. grid-item-card:: :octicon:`beaker` Design
      :link: design
      :link-type: doc

      Learn about the design philosophy and key features of ``pw_kernel``.

   .. grid-item-card:: :octicon:`milestone` Roadmap
      :link: roadmap
      :link-type: doc

      See the future direction and planned features for ``pw_kernel``.

.. TODO: https://pwbug.dev/424641732 - Re-enable the Rust API reference link.

.. toctree::
   :hidden:
   :maxdepth: 1

   quickstart
   guides
   design
   roadmap
   tooling/panic_detector/docs
