.. _showcase-sense-tutorial-bazel_cloud:

==============================
13. Use Bazel's cloud features
==============================
One of Bazel's defining features is that it's a cloud build system. Team
members can easily `share artifacts
<https://app.buildbuddy.io/invocation/f8bc4845-a38d-4c62-b939-14238168ba46>`__
(including test logs), builds can be run in parallel on hundreds of machines in
the cloud, and build artifacts that were built once (by anyone) don't need to
be rebuilt from scratch.

This section gives you a taste of these features of Bazel using `BuildBuddy
<https://www.buildbuddy.io/>`_.

----------------
BuildBuddy setup
----------------
To use cloud features, you need to get set up with some cloud provider.

.. note::

   Googlers: see additional instructions at `go/pw-sense-googlers
   <http://go/pw-sense-googlers#buildbuddy-integration>`_.

#. Go to https://app.buildbuddy.io/ and log in via Google or GitHub.
#. Click `Quickstart Guide <https://app.buildbuddy.io/docs/setup/>`__.
#. In step 1 (**Configure your .bazelrc**) enable the following options:

   * **API Key**
   * **Enable cache**
   * **Full cache**

   .. caution::

      :bug:`364781685`: Sense does not support remote execution yet, so don't
      enable that option.

#. Copy the provided snippet to ``//.bazelrc`` (the ``.bazelrc`` file in the root
   directory of your Sense repository).

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/buildbuddy_bazelrc_v1.png

#. Try :ref:`building the blinky app <showcase-sense-tutorial-build>` again to verify your
   BuildBuddy integration. While the build runs you should see an information log indicating
   that the build is streaming to BuildBuddy, like this:

   .. code-block:: text

      INFO: Streaming build results to: https://app.buildbuddy.io/invocation/6d467374-ffad-44be-a6be-e4f7b53129dd

---------------------
Review and share logs
---------------------
Let's go back to what we learned in :ref:`showcase-sense-tutorial-hosttests` and run a test.

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
   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         In **Bazel Build Targets** expand **//modules/blinky**, then right-click
         **:blinky_test (cc_test)**, then select **Test target**.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/test_target_v2.png
            :alt: Selecting Test target

            Starting ``blinky_test``

         A task launches a terminal. You should see ``blinky_test`` fail, and a
         BuildBuddy invocation link printed:

         .. code-block:: console

            //modules/blinky:blinky_test         FAILED in 0.4s
            INFO: Streaming build results to: https://app.buildbuddy.io/invocation/f8bc4845-a38d-4c62-b939-14238168ba46

      .. tab-item:: CLI
         :sync: cli

         Run the following command. You should see output similar to what's
         shown after the command. The key line starts with ``INFO: Streaming
         build results to:``. It contains the BuildBuddy invocation link.

         .. code-block:: console

            $ bazelisk test //modules/blinky:blinky_test
            # ...
            //modules/blinky:blinky_test         FAILED in 0.4s
              /home/kayce/.cache/bazel/_bazel_kayce/27fcdd448f61589ce2692618b3237728/execroot/showcase-rp2/bazel-out/k8-fastbuild/testlogs/modules/blinky/blinky_test/test.log

            Executed 1 out of 1 test: 1 fails locally.
            INFO: Streaming build results to: https://app.buildbuddy.io/invocation/f8bc4845-a38d-4c62-b939-14238168ba46

#. Click the provided BuildBuddy link. Some highlights to notice:

   * The **LOGS** tab provides a full log of the failed test.
   * The **DETAILS** tab shows the exact command line invocation for reproducing this result.
   * The **TIMING** tab shows a granular breakdown of how long Bazel took to build and execute the test.

#. Click **Share** (top-right corner) and broaden the permissions to make
   the invocation link shareable. Then show it off to your friends and
   coworkers!

--------------
Remote caching
--------------
Bazel supports remote caching: if you (or someone else in your organization)
already built an artifact, you can simply retrieve it from the cache instead of
building it from scratch. Let's test it out.


#. Remove all local Bazel build results.

   .. code-block:: console

      $ bazelisk clean

#. Run ``blinky_test`` again. It should be quite fast, even though you
   discarded all the build artifacts. Bazel simply downloads them from the
   remote cache that your previous invocation populated!

   You can click the **CACHE** tab in the BuildBuddy invocation UI to see more
   details on cache performance: how many hits there were, how much data was
   uploaded and downloaded, etc.

-------
Summary
-------
In this section, you've gotten a taste of the cloud features of Bazel:
generating easily shareable URLs for build and test invocations, and speeding
up your builds by leveraging remote caching.

Next, head over to :ref:`showcase-sense-tutorial-prod` to try
out the ``production`` app that exercises more of the Enviro+ Pack hardware.
