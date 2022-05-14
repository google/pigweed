.. _module-pw_third_party_googletest:

==========
GoogleTest
==========

The ``$dir_pw_third_party/googletest/`` module provides various helpers to
optionally use full upstream GoogleTest/GoogleMock with
:ref:`module-pw_unit_test`.

----------------------------------------
Using upstream GoogleTest and GoogleMock
----------------------------------------

If you want to use the full upstream GoogleTest/GoogleMock, you must do the
following:

.. note::

  Not all unit tests build properly with upstream GoogleTest yet. This is a
  work in progress.

GN
==
* Set the GN var ``dir_pw_third_party_googletest`` to the location of the
  GoogleTest source. You can use ``pw package install googletest`` to fetch the
  source if desired.
* Set the GN var ``pw_unit_test_MAIN = "//third_party/googletest:gmock_main"``.
* Set the GN var ``pw_unit_test_PUBLIC_DEPS = [ "//third_party/googletest" ]``.

CMake
-----
* Set the ``dir_pw_third_party_googletest`` to the location of the
  GoogleTest source. You can use ``pw package install googletest`` to fetch the
  source if desired.
* Set the ``pw_unit_test_MAIN`` to ``pw_third_party.googletest.gmock_main``.
* Set the GN var ``pw_unit_test_PUBLIC_DEPS`` to ``pw_third_party.googletest``.
