.. _docs-first-time-setup-windows:

===================================
Get started with Pigweed on Windows
===================================
.. _docs-first-time-setup-windows-express-setup:

-------------
Express setup
-------------
Run the following commands, and you should be ready to start developing with
Pigweed:

.. code-block:: bat

   curl https://pigweed.googlesource.com/pigweed/sample_project/+/main/tools/setup_windows_prerequisites.bat?format=TEXT > setup_pigweed_prerequisites.b64 && certutil -decode -f setup_pigweed_prerequisites.b64 setup_pigweed_prerequisites.bat && del setup_pigweed_prerequisites.b64
   setup_pigweed_prerequisites.bat

.. admonition:: Note
   :class: warning

   Due to issues with long file path handling on Windows, Pigweed strongly
   recommends you clone projects to a short path like ``C:\projects``.

------------------------------
Manual setup with explanations
------------------------------
This section expands the contents of the express setup into more detailed
explanations of Pigweed's Windows prerequisites. If you have already gone
through the :ref:`docs-first-time-setup-windows-express-setup`, you don't need
to go through these steps.

Install prerequisites
=====================
* Install `Git <https://git-scm.com/download/win>`_. Git must be installed to
  run from the command line and third-party software or be added to ``PATH``.
* Install `Python <https://www.python.org/downloads/windows/>`_.
* If you plan to flash devices with firmware, you'll need to install
  `OpenOCD <https://github.com/openocd-org/openocd/releases/latest>`_ and ensure
  it's on your system PATH.

Configure system settings
=========================
* Ensure that `Developer Mode
  <https://docs.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development>`_
  is enabled. This can also be done by running the following command as an
  administrator:

  .. code-block:: bat

     REG ADD HKLM\Software\Microsoft\Windows\CurrentVersion\AppModelUnlock /t REG_DWORD /v AllowDevelopmentWithoutDevLicense /d 1 /f\""

* Enable long file paths. This can be done using ``regedit`` or by running the
  following command as an administrator:

  .. code-block:: bat

     REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\FileSystem /v LongPathsEnabled /t REG_DWORD /d 1 /f

* Enable Git symlinks:

  .. code-block:: bat

     git config --global core.symlinks true

-------------
Support notes
-------------
While Windows is a supported platform for embedded software development with
Pigweed, the experience might not be quite as seamless when compared to macOS
and Linux. When developing on Windows, you can enjoy most of Pigweed's features
like automated on-device unit testing, RPC, and rich build system and IDE
integrations, but you may experience occasional snags along the way.

Long file path issues
=====================
Even though Pigweed's setup process enables long file path handling at a system
level, this doesn't mean that the problem is eliminated. Some applications are
hard-coded in ways that cause long file paths to continue to work incorrectly.

For example, `MinGW-w64 <https://www.mingw-w64.org/>`_-based GCC toolchains have
a `long-standing issue <https://issues.pigweed.dev/300995404>`_ with handling
long file paths on Windows. Unfortunately, this includes the Windows binaries
for `Arm's GNU toolchains <https://developer.arm.com/downloads/-/gnu-rm>`_.

For this reason, Pigweed strongly recommends cloning projects into a short path
like ``C:\projects``. It's also a good idea to be aware of the length of file
paths throughout your project.

Other limitations
=================
Due to the nature of OS implementation differences, the following features
are not currently supported on Windows:

* Pigweed does not provide integrations for
  `C++ sanitizers <https://github.com/google/sanitizers/wiki>`_ and
  `fuzz testing <https://github.com/google/fuzztest?tab=readme-ov-file#fuzztest>`_
  on Windows.
* :ref:`pw_build_info GNU build IDs <module-pw_build_info-gnu-build-ids>`: Not
  supported when building for Windows, but supported when building for embedded
  devices.
* :ref:`pw_tokenizer string tokenization <module-pw_tokenizer-tokenization>`:
  Not supported when building for Windows, but supported when building for
  embedded devices.

Individual modules have the most recent status on OS compatibility, so when in
doubt check the documentation for the module of interest.
