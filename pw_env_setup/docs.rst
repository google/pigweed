.. _chapter-pw-env_setup:

------------
pw_env_setup
------------
A classic problem in the embedded space is reducing the time from git clone
to having a binary executing on a device. The issue is that an entire suite
of tools is needed for non-trivial production embedded projects. For example:

 - A C++ compiler for your target device, and also for your host
 - A build system or three; for example, GN, Ninja, CMake, Bazel
 - A code formatting program like clang-format
 - A debugger like OpenOCD to flash and debug your embedded device
 - A known Python version with known modules installed for scripting
 - A Go compiler for the Go-based command line tools

...and so on

In the server space, container solutions like Docker or Podman solve this;
however, in our experience container solutions are a mixed bag for embedded
systems development where one frequently needs access to native system
resources like USB devices, or must operate on Windows.

``pw_env_setup`` is our compromise solution for this problem that works on Mac,
Windows, and Linux. It leverages the Chrome packaging system `CIPD`_ to
bootstrap a Python installation, which in turn inflates a virtual
environment. The tooling is installed into your workspace, and makes no
changes to your system. This tooling is designed to be reused by any
project.

.. _CIPD: https://github.com/luci/luci-go/tree/master/cipd

By default packages will be installed in a ``.environment`` folder within the
checkout root, and CIPD will cache files in ``$HOME/.cipd-cache-dir``. These
paths can be overridden by setting ``PW_ENVIRONMENT_ROOT`` and
``CIPD_CACHE_DIR``, respectively.

.. warning::
  At this time ``pw_env_setup`` works for us, but isn’t well tested. We don’t
  suggest relying on it just yet. However, we are interested in experience
  reports; if you give it a try, please `send us a note`_ about your
  experience.

.. _send us a note: pigweed@googlegroups.com

Projects using Pigweed can leverage ``pw_env_setup`` to install their own
dependencies. The following environment variables are now used to pass options
into pw_env_setup.

    * ``PW_CIPD_PACKAGE_FILES``
    * ``PW_VIRTUALENV_REQUIREMENTS``
    * ``PW_VIRTUALENV_SETUP_PY_ROOTS``
    * ``PW_CARGO_PACKAGE_FILES``

Each of these variables can contain multiple entries separated by ``:``
(or ``;`` on Windows) like the ``PATH`` environment variable. However, they
will also be interpreted as globs, so
``PW_VIRTUALENV_REQUIREMENTS="/foo/bar/*/requirements.txt"`` is perfectly
valid. They should be full paths.

Projects depending on Pigweed should set these variables and then invoke
Pigweed's ``bootstrap.sh`` (or ``bootstrap.bat``), which will add to each of
these variables before invoking ``pw_env_setup``. Users wanting additional
setup can set these variables in their shell init files. Pigweed will add to
these variables and will not remove any existing values. At the end of
Pigweed's bootstrap process, it will reset these variables to their initial
values.
