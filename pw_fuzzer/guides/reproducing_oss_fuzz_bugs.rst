.. _module-pw_fuzzer-guides-reproducing_oss_fuzz_bugs:

=============================================
pw_fuzzer: Reproducing Bugs Found by OSS-Fuzz
=============================================
.. pigweed-module-subpage::
   :name: pw_fuzzer
   :tagline: Better C++ code through easier fuzzing

.. TODO: b/281139237 - Update with better instructions for downstream projects.

Core Pigweed is integrated with `OSS-Fuzz`_, a continuous fuzzing infrastructure
for open source software. Fuzzers listed in in ``pw_test_groups`` will
automatically start being run within a day or so of appearing in the git
repository.

Bugs produced by OSS-Fuzz can be found in its `Monorail instance`_. These bugs
include:

* A detailed report, including a symbolized backtrace.
* A revision range indicating when the bug has been detected.
* A minimized testcase, which is a fuzzer input that can be used to reproduce
  the bug.

To reproduce a bug:

#. Build the fuzzers.
#. Download the minimized testcase.
#. Run the fuzzer with the testcase as an argument.

For example, if the testcase is saved as ``~/Downloads/testcase``
and the fuzzer is the same as in the examples above, you could run:

.. code-block::

   $ ./out/host/obj/pw_fuzzer/toy_fuzzer ~/Downloads/testcase

If you need to recreate the OSS-Fuzz environment locally, you can use its
documentation on `reproducing`_ issues.

In particular, you can recreate the OSS-Fuzz environment using:

.. code-block::

   $ python infra/helper.py pull_images
   $ python infra/helper.py build_image pigweed
   $ python infra/helper.py build_fuzzers --sanitizer <address/undefined> pigweed

With that environment, you can run the reproduce bugs using:

.. code-block::

   python infra/helper.py reproduce pigweed <pw_module>_<fuzzer_name> ~/Downloads/testcase

You can even verify fixes in your local source checkout:

.. code-block::

   $ python infra/helper.py build_fuzzers --sanitizer <address/undefined> pigweed $PW_ROOT
   $ python infra/helper.py reproduce pigweed <pw_module>_<fuzzer_name> ~/Downloads/testcase

.. _Monorail instance: https://bugs.chromium.org/p/oss-fuzz
.. _OSS-Fuzz: https://github.com/google/oss-fuzz
.. _reproducing: https://google.github.io/oss-fuzz/advanced-topics/reproducing/
