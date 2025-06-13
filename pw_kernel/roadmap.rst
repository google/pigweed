.. _module-pw_kernel-roadmap:

=======
Roadmap
=======
.. pigweed-module-subpage::
   :name: pw_kernel

.. note::

   This is an early draft. The content may change significantly over the
   next few months.

This document outlines the future direction and planned features for
``pw_kernel``. As an experimental project, this roadmap is subject to change
based on ongoing research and community feedback.

Near-term goals
---------------
- **Enhanced memory protection**: Maturing the MPU/PMP support for more
  robust process isolation and defining a clear model for peripheral access
  from userspace.
- **Expanded driver model**: Developing a more comprehensive driver model that
  supports both kernel-mode and user-mode drivers, with clear APIs for
  both.
- **Improved syscall interface**: Refining the system call API for
  greater efficiency and an expanded feature set.

Long-term vision
----------------
- **Wider architecture support**: Enabling out-of-tree support for new and
  proprietary architectures.
- **Certification**: Continuing to work towards a clear path for
  certifying ``pw_kernel`` for use in safety-critical systems.
- **Power management**: Implementing advanced power management features for
  low-power applications.
