.. _docs-first-time-setup-macos:

=================================
Get started with Pigweed on macOS
=================================
.. _docs-first-time-setup-macos-express-setup:

-------------
Express setup
-------------
Run the following commands, and you should be ready to start developing with
Pigweed:

.. inclusive-language: disable

.. code-block:: sh

   xcode-select --install
   pyv=`python3 -c "import sys; v=sys.version_info; print('{0}.{1}'.format(v[0], v[1]))"`; /Applications/Python\ $pyv/Install\ Certificates.command
   sudo spctl --master-disable

.. inclusive-language: enable

------------------------------
Manual setup with explanations
------------------------------
This section expands the contents of the express setup into more detailed
explanations of Pigweed's macOS prerequisites. If you have already gone through
the :ref:`docs-first-time-setup-macos-express-setup`, you don't need to go
through these steps.

Install prerequisites
=====================
Xcode SDK
---------
Pigweed requires Xcode to build on macOS. While you don't need the full Xcode
SDK, you should at least have ``xcode-select``.

You can install ``xcode-select`` with the following command:

.. code-block:: sh

  xcode-select --install

SSL certificates
----------------
Pigweed's bootstrap process requires a working version of Python that can make
HTTPS requests to kickstart the initial dependency fetches. By default, the
macOS system Python does not have SSL certificates installed. You can install
them with the following commands:

.. code-block:: sh

   pyv=`python3 -c "import sys; v=sys.version_info; print('{0}.{1}'.format(v[0], v[1]))"`; /Applications/Python\ $pyv/Install\ Certificates.command

Configure system settings
=========================
Pigweed relies on many tools not downloaded from the App Store. While you may
prefer to manually allow individual applications, this may be frustrating or
impractical due to the large number of tools required to build Pigweed.

It is usually most practical to globally allow tools not originating from the
App Store using the following command:

.. inclusive-language: disable

.. code-block:: sh

   sudo spctl --master-disable

.. inclusive-language: enable

-------------
Support notes
-------------
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
