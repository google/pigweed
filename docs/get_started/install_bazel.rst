.. _docs-install-bazel:

=============
Install Bazel
=============
This page provides recommendations on how to install Bazel
for Bazel-based Pigweed projects.

.. _docs-install-bazel-bazelisk:

-----------------------------------
CLI commands: bazelisk versus bazel
-----------------------------------
Pigweed recommends always running ``bazelisk`` rather than ``bazel`` because
``bazelisk`` ensures that you always run the correct Bazel version defined in
your project's ``.bazelversion`` file.

-----------------
Recommended setup
-----------------
If you use Visual Studio (VS) Code, Pigweed's VS Code extension can manage
Bazel for you:

.. _Pigweed extension: https://marketplace.visualstudio.com/items?itemName=pigweed.pigweed-vscode

#. Get `VS Code <https://code.visualstudio.com/download>`_.
#. Install the `Pigweed extension`_ for VS Code.

------------------
Alternative setups
------------------

.. _Bazelisk release: https://github.com/bazelbuild/bazelisk/releases

.. _Bazelisk repo: https://github.com/bazelbuild/bazelisk

.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      .. caution::

         Most Linux distributions don't have a recent version of Bazelisk
         in their software repositories, or they don't have it at all.

      Install from a binary release:

      #. Get the latest `Bazelisk release`_.

      #. Add the binary to your ``PATH``.

      Install from source:

      #. Install `Go <https://go.dev/doc/install>`_.

      #. Clone the source:

         .. code-block::

            go install github.com/bazelbuild/bazelisk@latest

      #. Add the Go binaries to your ``PATH``:

         .. code-block::

            export PATH=$PATH:$(go env GOPATH)/bin

   .. tab-item:: macOS
      :sync: macos

      (Recommended) `Homebrew <https://brew.sh/>`_:

      .. code-block::

         brew install bazelisk

      `MacPorts <https://www.macports.org/install.php>`_:

      .. code-block::

         sudo port install bazelisk

   .. tab-item:: Windows
      :sync: windows

      (Recommended) `winget <https://learn.microsoft.com/en-us/windows/package-manager/winget/>`_:

      .. code-block::

         winget install --id=Bazel.Bazelisk  -e

      `Chocolatey <https://chocolatey.org/install>`_:

      .. code-block::

         choco install bazelisk
