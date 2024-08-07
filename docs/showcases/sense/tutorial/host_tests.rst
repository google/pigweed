.. _showcase-sense-tutorial-hosttests:

=================
5. Run host tests
=================
.. warning::

   (2024 August 7) This tutorial doesn't quite work yet.
   Check back tomorrow!

:ref:`module-pw_unit_test` provides an extensive GoogleTest-compatible
unit testing framework. Before building and running the app, let's first
verify that the app's logic is correct by exercising the app's unit
tests:

#. Open ``//modules/blinky/blinky_test.cc``.

#. Make the ``Toggle`` test fail by changing one of the expected values.
   Example:

   .. code-block:: c++

      TEST_F(BlinkyTest, Toggle) {
        // ...
        auto event = FirstActive();
        ASSERT_NE(event, monochrome_led_.events().end());
        EXPECT_EQ(event->state, State::kInactive);   // add this line
        // EXPECT_EQ(event->state, State::kActive);  // comment out this line
        EXPECT_GE(ToMs(event->timestamp - start), kIntervalMs * 0);
        start = event->timestamp;
        // ...
      }

   .. caution:: Remember to save your changes!

#. Run the tests:

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         In **Bazel Build Targets** expand **//modules/blinky**, then right-click
         **:blinky_test (cc_test)**, then select **Test target**.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/test_target_v2.png
            :alt: Selecting Test target

            Starting ``blinky_test``

         A task launches a terminal. You should see ``blinky_test`` fail:

         .. code-block:: console

            //modules/blinky:blinky_test         FAILED in 0.4s

         Press any key to close the terminal that the task launched.

         .. tip::

            ``bazelisk test //...`` runs all unit tests. This command
            needs to be run from a terminal with ``bazelisk`` on its path.
            The Pigweed extension for VS Code automatically downloads
            ``bazelisk`` for you and puts it on your VS Code terminal path
            so that you can use ``bazelisk`` from VS Code terminal without
            any manual setup.

      .. tab-item:: CLI
         :sync: cli

         Run the following command. You should see output similar to what's
         shown after the command. The key line is
         ``Executed 1 out of 1 test: 1 fails locally.``

         .. code-block:: console

            $ bazelisk test //modules/blinky:blinky_test
            # ...
            //modules/blinky:blinky_test         FAILED in 0.4s
              /home/kayce/.cache/bazel/_bazel_kayce/27fcdd448f61589ce2692618b3237728/execroot/showcase-rp2/bazel-out/k8-fastbuild/testlogs/modules/blinky/blinky_test/test.log

            Executed 1 out of 1 test: 1 fails locally.
            # ...

         .. tip::

            To run all host tests, call ``bazelisk test //...``.

#. Revert the test to its original state.

#. Run the tests again and make sure they pass this time.

   You should see ``blinky_test`` pass this second time:

   .. code-block:: console

      //modules/blinky:blinky_test         PASSED in 0.4s

.. note::

   If you see warnings that begin with ``There were tests whose
   specified size is too big``, you can ignore them. If you encounter this warning
   in your own project, it means you need to adjust the timeout of the tests.

.. _showcase-sense-tutorial-hosttests-summary:

-------
Summary
-------
You might have found it a little strange (and boring) that we're showing you
unit tests right now, rather than demo'ing apps. We're getting to the
fun stuff soon, we promise! The reason we showed you testing now is
because it's a very important part of Pigweed's :ref:`mission <docs-mission>`.
When you're on a large embedded development team creating a new product, it's
so much easier to iterate quickly when you have confidence that your code
changes aren't introducing bugs in other parts of the codebase. The best way
to build that confidence is to rigorously test every part of your codebase.
Pigweed spends a lot of time making it easier for teams to test their
codebases, such as making it possible to run unit tests on your development
host rather than on physical hardware. This is especially useful when
your physical hardware doesn't exist yet because your hardware team
hasn't finished designing it!

Another reason why it's important to make host-portable code:
security and robustness. This enables us to run modern code analysis
tooling like ASAN, TSAN, MSAN, fuzzers, and more against Sense. These
tools are unlikely to run correctly in on-device embedded contexts.
Fun fact: We caught real bugs in Sense with this tooling during
development!

As promised, now it's time for the fun stuff. Head over to
:ref:`showcase-sense-tutorial-sim` to start trying out the bringup
app (``blinky``).
