.. _showcase-sense-tutorial-automate:

=============================
10. Automate common workflows
=============================
If you find yourself repeating the same sequence of steps in
``pw_console`` over and over again, there are a few ways to automate
those workflows.

.. _showcase-sense-tutorial-automate-snippets:

--------
Snippets
--------
Teams often have common commands or RPCs they need to run while
debugging or developing a product. You can put these in snippets
for easy sharing across your team. Try running a snippet now:

#. In ``pw_console`` click **File**.

   .. Add a screenshot here that makes it easier to find File menu

#. Click **Insert Repl Snippet**.

#. Select **Echo RPC** with your keyboard and then press :kbd:`Enter`.

   .. admonition:: Troubleshooting

      If clicking **Echo RPC** doesn't work, try the keyboard-based
      workflow.

   You should see the **Python Repl** input prompt get auto-populated
   with an echo command.

#. Focus the **Python Repl** and then press :kbd:`Enter` to execute
   the pre-populated echo command.

#. If your curious about how snippets are implemented, take a look
   at ``//.pw_console.yaml``. Notice the ``snippets`` entries. Each
   of these is an automated workflow that can be run in ``pw_console``.

#. Press :kbd:`Ctrl+D` twice to close ``pw_console``.

The following video is a demonstration of snippets:

.. raw:: html

   <video preload="metadata" style="width: 100%; height: auto;" controls>
     <source type="video/webm"
             src="https://storage.googleapis.com/pigweed-media/airmaranth/snippets.webm#t=14.0"/>
   </video>

.. _showcase-sense-tutorial-automate-scripts:

--------------
Python scripts
--------------
For a long, complex workflow, you may prefer encapsulating
the workflow into its own script.

#. Run the script:

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         In **Bazel Build Targets** expand **//tools**, then
         right-click **:example_script (py_binary)**, then select
         **Run target**.

         A terminal launches and executes the script contained in
         ``//tools/sense/example_script.py``.

      .. tab-item:: CLI
         :sync: cli

         Run the following command:

         .. code-block:: console

            $ bazelisk run //tools:example_script

.. admonition:: Troubleshooting

   If the script fails: make sure that you close the ``pw_console``
   from the last section before attempting this section.

You should see output similar to this:

.. code-block:: console

   # ...
   The status was Status.OK
   The message was Hello
   20240717 18:26:25 INF Calling Echo(msg="Goodbye!")
   20240717 18:26:25 DBG Starting PendingRpc(channel_id=1, service_id=352047186, method_id=2336689897, call_id=3)
   20240717 18:26:25 DBG PendingRpc(channel_id=1, service_id=352047186, method_id=2336689897, call_id=3) finished with status Status.OK
   20240717 18:26:25 DBG PendingRpc(channel_id=1, service_id=352047186, method_id=2336689897, call_id=3) received response: msg: "Goodbye!"

   20240717 18:26:25 INF PendingRpc(channel_id=1, service_id=352047186, method_id=2336689897, call_id=3) completed: Status.OK
   Status.OK: msg: "Goodbye!"

Towards the end of the output you can see the echoed message and the
printed status information.

Take a look at ``//tools/sense/example_script.py`` if you're
curious about how this script is implemented.

.. _showcase-sense-tutorial-automate-summary:

-------
Summary
-------
Gone are the days of ad hoc development workflows that some
teammates benefit from and others don't. With Pigweed, these
common workflows become explicit, centralized, and shareable,
and they're checked in alongside the rest of the project's
code.

Next, head over to :ref:`showcase-sense-tutorial-webapp` to try
interacting with your Pico through a web app.
