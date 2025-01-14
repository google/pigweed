.. _module-pw_sys_io_stdio:

---------------
pw_sys_io_stdio
---------------
.. pigweed-module::
   :name: pw_sys_io_stdio

The ``pw_sys_io_stdio`` backend implements the ``pw_sys_io`` facade using
stdio.

Why not just use stdio directly?
--------------------------------
The nice thing about using ``pw_sys_io`` is that it's rather easy to get a
board up and running with a target-specific backend. This means when drafting
out a quick application you can write it against ``pw_sys_io`` and, with some
care, the application will be able to run on both host and target devices.

While it's not recommended to use ``pw_sys_io`` for any production
applications, it can be rather helpful for early prototyping.

Setup
=====
This module requires relatively minimal setup:

1. Write code against the ``pw_sys_io`` facade.
2. Setup build system to point to this backend.

 - For GN, direct the ``pw_sys_io_BACKEND`` GN build arg to point here.
 - For Bazel, the default backend multiplexer will automatically pick this stdio
   backend for host builds. To use this backend on non-host platforms, add the
   ``@pigweed//pw_sys_io_stdio:backend`` constraint_value to your platform.

Module usage
============
For the most part, applications built with this backend will behave similarly
to an application built directly against stdio.

Dependencies
============
- :ref:`module-pw_sys_io`
