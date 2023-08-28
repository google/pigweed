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

Next, set the GN variable ``dir_pw_third_party_emboss`` to the path of your Emboss
installation. If using the submodule path from above, add the following to the
``default_args`` of your project's ``.gn`` file:

.. code-block::

   dir_pw_third_party_emboss = "//third_party/emboss/src"

..
   inclusive-language: disable

Optionally, configure the Emboss defines documented at
`dir_pw_third_party_emboss/runtime/cpp/emboss_defines.h
<https://github.com/google/emboss/blob/master/runtime/cpp/emboss_defines.h>`_
by setting the ``pw_third_party_emboss_CONFIG`` variable to a source set that
includes a public config overriding the defines. By default, checks will
use PW_DASSERT.

..
   inclusive-language: enable

------------
Using Emboss
------------
Let's say you've authored an Emboss source file at ``//some/path/to/my-protocol.emb``.
To generate its bindings, you can add the following to ``//some/path/to/BUILD.gn``:

.. code-block::

  import("$dir_pw_third_party/emboss/build_defs.gni")

  emboss_cc_library("protocol") {
    source = "my-protocol.emb"
  }

This generates a source set of the same name as the target, in this case "protocol".
To use the bindings, list this target as a dependency in GN and include the generated
header by adding ``.h`` to the path of your Emboss source file:

.. code-block::

  #include <some/path/to/protocol.emb.h>
