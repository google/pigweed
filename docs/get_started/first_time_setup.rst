.. _docs-first-time-setup-guide:

========================
Get started with Pigweed
========================
.. _docs-first-time-setup-guide-express-setup:

-------------
Express setup
-------------
Run the following commands, and you should be ready to start developing with
Pigweed:

.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      .. inclusive-language: disable

      .. code-block:: sh

         sudo apt install git build-essential
         curl -LJo /etc/udev/rules.d/60-openocd.rules https://raw.githubusercontent.com/openocd-org/openocd/master/contrib/60-openocd.rules

      .. inclusive-language: enable

      .. admonition:: Note
         :class: tip

         If you're using a Linux distribution that isn't based on Debian/Ubuntu,
         see the manual setup directions below for prerequisite installation
         instructions.

   .. tab-item:: macOS
      :sync: macos

      .. inclusive-language: disable

      .. code-block:: sh

         xcode-select --install
         pyv=`python3 -c "import sys; v=sys.version_info; print('{0}.{1}'.format(v[0], v[1]))"`; /Applications/Python\ $pyv/Install\ Certificates.command
         sudo spctl --master-disable

      .. inclusive-language: enable

   .. tab-item:: Windows
      :sync: windows

      .. code-block:: bat

         curl https://pigweed.googlesource.com/pigweed/examples/+/main/tools/setup_windows_prerequisites.bat?format=TEXT > setup_pigweed_prerequisites.b64 && certutil -decode -f setup_pigweed_prerequisites.b64 setup_pigweed_prerequisites.bat && del setup_pigweed_prerequisites.b64
         setup_pigweed_prerequisites.bat

      .. admonition:: Note
         :class: warning

         Due to issues with long file path handling on Windows, Pigweed strongly
         recommends you clone projects to a short path like ``C:\projects``.

------------------------------
Manual setup with explanations
------------------------------
This section expands the contents of the express setup into more detailed
explanations of Pigweed's prerequisites. If you have already gone through the
:ref:`docs-first-time-setup-guide-express-setup`, you don't need to go through
these steps.

Install prerequisites
=====================


.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      Most Linux installations should work out-of-the-box and not require any manual
      installation of prerequisites beyond basics like ``git`` and
      ``build-essential`` (or the equivalent for your distro). If you already do
      software development, you likely already have these installed.

      To ensure you have the necessary prerequisites, you can run the following
      command on Debian/Ubuntu-based distributions:

      .. code-block:: sh

         sudo apt install git build-essential

      The equivalent command on Fedora-based distributions is:

      .. code-block:: sh

         sudo dnf groupinstall "Development Tools"

      The equivalent command on Arch-based distributions is:

      .. code-block:: sh

         sudo pacman -S git base-devel

   .. tab-item:: macOS
      :sync: macos

      **Xcode SDK**

      Pigweed requires Xcode to build on macOS. While you don't need the full Xcode
      SDK, you should at least have ``xcode-select``.

      You can install ``xcode-select`` with the following command:

      .. code-block:: sh

         xcode-select --install

      **SSL certificates**

      Pigweed's bootstrap process requires a working version of Python that can make
      HTTPS requests to kickstart the initial dependency fetches. By default, the
      macOS system Python does not have SSL certificates installed. You can install
      them with the following commands:

      .. code-block:: sh

         pyv=`python3 -c "import sys; v=sys.version_info; print('{0}.{1}'.format(v[0], v[1]))"`; /Applications/Python\ $pyv/Install\ Certificates.command

   .. tab-item:: Windows
      :sync: windows

      * Install `Git <https://git-scm.com/download/win>`_. Git must be installed to
        run from the command line and third-party software or be added to ``PATH``.
      * Install `Python <https://www.python.org/downloads/windows/>`_.
      * If you plan to flash devices with firmware, you'll need to
        `install OpenOCD <https://github.com/openocd-org/openocd/releases/latest>`_
        and ensure it's on your system PATH.



Configure system settings
=========================

.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      .. inclusive-language: disable

      To flash devices using `OpenOCD <https://openocd.org/>`_, you will need to
      extend your system udev rules by adding a new configuration file in
      ``/etc/udev/rules.d/`` that lists the hardware debuggers you'll be using. The
      OpenOCD repository has a good
      `example udev rules file <https://github.com/openocd-org/openocd/blob/master/contrib/60-openocd.rules>`_
      that includes many popular hardware debuggers.

      .. inclusive-language: enable

   .. tab-item:: macOS
      :sync: macos

      Pigweed relies on many tools not downloaded from the App Store. While you may
      prefer to manually allow individual applications, this may be frustrating or
      impractical due to the large number of tools required to build Pigweed.

      It is usually most practical to globally allow tools not originating from the
      App Store using the following command:

      .. inclusive-language: disable

      .. code-block:: sh

         sudo spctl --master-disable

      .. inclusive-language: enable

   .. tab-item:: Windows
      :sync: windows

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

.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      Linux is Pigweed's recommended platform for embedded software development. When
      developing on Linux, you can enjoy all of Pigweed's benefits like tokenized
      logging, automated on-device unit testing, RPC, and rich build system and IDE
      integrations.

   .. tab-item:: macOS
      :sync: macos

      macOS is a well-supported platform for embedded software development with
      Pigweed. When developing on macOS, you can enjoy the vast majority of benefits
      of Pigweed like automated on-device unit testing, RPC, and rich build system
      and IDE integrations.

      Due to the nature of OS implementation differences, the following features
      are not supported on macOS:

      * :ref:`pw_build_info GNU build IDs <module-pw_build_info-gnu-build-ids>`: Not
        supported when building for macOS, but supported when building for embedded
        devices.
      * :ref:`pw_tokenizer string tokenization <module-pw_tokenizer-tokenization>`:
        Not supported when building for macOS, but supported when building for
        embedded devices.

      Individual modules have the most recent status on OS compatibility, so when in
      doubt check the documentation for the module of interest.

   .. tab-item:: Windows
      :sync: windows

      While Windows is a supported platform for embedded software development with
      Pigweed, the experience might not be quite as seamless when compared to macOS
      and Linux. When developing on Windows, you can enjoy most of Pigweed's features
      like automated on-device unit testing, RPC, and rich build system and IDE
      integrations, but you may experience occasional snags along the way.

      **Long file path issues**

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

      **Other limitations**

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
