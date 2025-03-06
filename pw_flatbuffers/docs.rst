.. _module-pw_flatbuffers:

==============
pw_flatbuffers
==============
.. pigweed-module::
   :name: pw_flatbuffers

The flatbuffers module supports the use of cmake to build C++ headers from a
set of flatbuffers schema files. The user of these build files may either use
the provided flatbuffers distribution or provide their own.

-----------------------
Flatbuffers compilation
-----------------------
pw_flatbuffers currently only supports ``cmake`` as the build system.

CMake
=====
CMake provides a ``pw_flatbuffer_library`` function to build flatbuffer schema
files into C++ headers. The user of this function is expected to configure the
flatbuffers compiler as well as provide any needed dependencies such as
flatbuffers headers. Support for outputting other languages supported by
``flatc`` is currently not implemented.


**Arguments**

* ``NAME``: Name of dependency to create. The dependency target name will be
  ``NAME.cpp``
* ``SOURCES``: Source schema files to compile to headers
* ``DEPS``: Dependencies needed to compile the passed in schema files
* ``FLATC_FLAGS``: flags to be passed to ``flatc`` when building

**Globals**

Additionally, several global cache variables influence the behavior of
``pw_flatbuffer_library``.

* ``pw_flatbuffers_FLATC``: Path to the flatbuffers compiler executable. This
  defaults to simply using ``flatc`` and relying on the shell ``PATH`` to
  locate the compiler.
* ``pw_flatbuffers_FLATC_FLAGS``: Global flags to pass to all invocations of
  the ``flatc`` compiler. Defaults to empty.
* ``pw_flatbuffers_LIBRARY``: Flatbuffer header library dependency to
  automatically add to all flatbuffers libraries built. Defaults to unset
  unless using the pigweed provided flatbuffers by setting
  ``dir_pw_third_party_flatbuffers``, in which case this will default to the
  pigweed provided third party flatbuffers. If left unset, the user takes
  responsibility for properly providing the flatbuffer dependencies to code.

**Example***

.. code_block: cmake

  # Point to our installed directory
  set(dir_pw_third_party_flatbuffers ${path_to_flatbuffers_install})

  # Set global flags, such as the C++ version
  set(pw_flatbuffers_FLATC_FLAGS --cpp-std c++11 CACHE LIST "Global args to flatc")

  include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
  include($ENV{PW_ROOT}/pw_flatbuffers/flatbuffers.cmake)

  # Build the fbs files to headers
  pw_flatbuffer_library(simple_test_schema
    SOURCES
      pw_flatbuffers_test_schema/simple_test.fbs
  )

  # Build a test exe
  pw_add_test(pw_flatbuffers.simple_test
    SOURCES
      simple_test.cc
    PRIVATE_DEPS
      simple_test_schema.cpp
    GROUPS
      modules
  )


GN
==
GN is not currently supported.

Bazel
=====
Bazel is not currently supported.
