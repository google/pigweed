.. _module-pw_third_party_emboss:

======
Emboss
======
`Emboss <https://github.com/google/emboss>`_ is a tool for generating code to
safely read and write binary data structures.

The ``$dir_pw_third_party/emboss`` module provides an ``emboss_cc_library`` GN
template, defined in build_defs.gni, which generates C++ bindings for the given
Emboss source file. The Emboss source code needs to be provided by the user.

------------------
Configuring Emboss
------------------
The recommended way to include the Emboss source code is to add it as a
Git submodule:

.. code-block:: sh

   git submodule add https://github.com/google/emboss.git third_party/emboss/src

.. tab-set::

   .. tab-item:: GN

      Next, set the GN variable ``dir_pw_third_party_emboss`` to the path of
      your Emboss installation. If using the submodule path from above, add the
      following to the ``default_args`` of your project's ``.gn`` file:

      .. code-block::

         dir_pw_third_party_emboss = "//third_party/emboss/src"

      ..
         inclusive-language: disable

      Optionally, configure the Emboss defines documented at
      `dir_pw_third_party_emboss/runtime/cpp/emboss_defines.h
      <https://github.com/google/emboss/blob/master/runtime/cpp/emboss_defines.h>`_
      by setting the ``pw_third_party_emboss_CONFIG`` variable to a source set
      that includes a public config overriding the defines. By default, checks
      will use PW_DASSERT.

      ..
         inclusive-language: enable

   .. tab-item:: CMake

      Next, set the CMake variable ``dir_pw_third_party_emboss`` to the path of
      your Emboss installation. If using the submodule path from above, add the
      following to your project's ``CMakeLists.txt`` file:

      .. code-block::

         set(dir_pw_third_party_emboss "$ENV{PW_ROOT}/third_party/emboss/src" CACHE PATH "" FORCE)


------------
Using Emboss
------------
.. tab-set::

   .. tab-item:: GN

      Let's say you've authored an Emboss source file at
      ``//my-project/public/my-project/my-protocol.emb``.
      To generate its bindings, you can add the following to
      ``//my-project/BUILD.gn``:

      .. code-block::

         import("$dir_pw_third_party/emboss/build_defs.gni")

         emboss_cc_library("emboss_protocol") {
            source = "public/my-project/my-protocol.emb"
         }

      This generates a source set of the same name as the target, in this case
      "emboss_protocol". To use the bindings, list this target as a dependency
      in GN:

      .. code-block::

         pw_test("emboss_test") {
            sources = [ "emboss_test.cc" ]
            deps = [
               ":emboss_protocol",
            ]
         }

   .. tab-item:: CMake

      Let's say you've authored an Emboss source file at
      ``my-project/public/my-project/my-protocol.emb``.
      To generate its bindings, you can add the following to
      ``my-project/CMakeLists.txt``:

      .. code-block::

         include($ENV{PW_ROOT}/third_party/emboss/emboss.cmake)

         emboss_cc_library(emboss_protocol
           SOURCES
             source = "public/my-project/my-protocol.emb"
         )

      This generates a library of the same name as the target, in this case
      "emboss_protocol". To use the bindings, list this target as a dependency
      in CMake:

      .. code-block::

         pw_add_test(emboss_test
            SOURCES
               emboss_test.cc
            PRIVATE_DEPS
               emboss_protocol
         )

Now just include the generated header by adding ``.h`` to the path of your
Emboss source file:

.. code-block:: c++

   #include <my-project/my-protocol.emb.h>
