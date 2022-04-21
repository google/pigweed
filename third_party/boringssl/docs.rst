.. _module-pw_third_party_boringssl:

=========
BoringSSL
=========

The ``$dir_pw_third_party/boringssl`` module provides the build files to
compile and use BoringSSL. The source code of BoringSSL needs to be provided by
the user. It is recommended to download it via :ref:`module-pw_package`

-------------
Build support
-------------

This module provides support to compile BoringSSL with GN. This is required when
compiling backends modules that use BoringSSL, such as some facades in
:ref:`module-pw_crypto`

``pw package``
==============

You can install BoringSSL source code as part of the bootstrap process with
``pw package``, which is the recommended method. There are two ways to set it
up:

#. As part of the bootstrap process, add "boringssl" to the list of pw_packages
   in your project's
   ``env_setup.json`` file:

   .. code-block:: json

     {
       "pw_packages": [
            "boringssl"
        ]
     }

#. Manually, run ``pw package install boringssl``. Every user would need to run
   this command at least once.

GN
==
The GN build file depends on a generated file called ``BUILD.generated.gni``
with the list of the different types of source files for the selected BoringSSL
version. ``pw package`` generates this file as part of the install process. If
you provide the source code in a different way, make sure this file is
generated.

The GN variables needed are defined in
``$dir_pw_third_party/boringssl/boringssl.gni``:

#. Set the GN ``dir_pw_third_party_boringssl`` to the path of the BoringSSL
   installation.

   - If using ``pw package``, add the following to the ``default_args`` in the
     project's ``.gn``:

     .. code-block::

       dir_pw_third_party_boringssl =
           getenv("_PW_ACTUAL_ENVIRONMENT_ROOT") + "/packages/boringssl"

     The packages are normally installed at ``//.environment/packages``, but if
     you are using Pigweed's Commit Queue infrastructure this location is
     different.

#. Having a non-empty ``dir_pw_third_party_boringssl`` variable causes GN to
   attempt to include the ``BUILD.generated.gni`` file from the sources even
   during the bootstrap process before the source package is installed by the
   bootstrap process. To avoid this problem, set this variable to the empty
   string during bootstrap by adding it to the ``virtualenv.gn_args`` setting in
   the ``env_setup.json`` file:

   .. code-block:: json

     {
       "virtualenv": {
         "gn_args": [
           "dir_pw_third_party_boringssl=\"\""
         ]
       }
     }


After this is done a ``pw_source_set`` for the BoringSSL library is created at
``$dir_pw_third_party/borignssl``.
