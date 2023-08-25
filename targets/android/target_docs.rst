.. _target-android:

-------
Android
-------
The Android target supports building Pigweed on devices using an Android
Native Development Kit (NDK).

.. warning::
  This target is under construction, not ready for use, and the documentation
  is incomplete.

Setup
=====
You must first download and unpack a copy of the `Android NDK`_ and let Pigweed
know where that is located using the ``pw_android_toolchain_NDK_PATH`` build
arg.

.. _Android NDK: https://developer.android.com/ndk

You can set Pigweed build options using ``gn args out``.

Building
========
To build for this Pigweed target, simply build the top-level "android" Ninja
target. You can set Pigweed build options using ``gn args out`` or by running:

.. code-block:: sh

  gn gen out --args='
    pw_android_toolchain_NDK_PATH="/path/to/android/ndk"'

On a Windows machine it's easier to run:

.. code-block:: sh

  gn args out

That will open a text file where you can paste the args in:

.. code-block:: text

  pw_android_toolchain_NDK_PATH = "/path/to/android/ndk"

Save the file and close the text editor.

Then build with:

.. code-block:: sh

  ninja -C out android

This will build Pigweed for all supported Android CPU targets at the default
optimization level, currently arm, arm64, x64, and x86.

To build for a specific CPU target only, at the default optimization level:

.. code-block:: sh

  ninja -C out arm64_android

Or to build for a specific CPU target and optimization level:

.. code-block:: sh

  ninja -C out arm64_android_size_optimized
