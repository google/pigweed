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

#. If you don't already have a ``pw_console`` instance connected to
   a Pico, connect one now:

   .. tab-set::

      .. tab-item:: Pico 1 & 1W (RP2040)

         .. code-block:: console

            bazelisk run //apps/blinky:rp2040_console

      .. tab-item:: Pico 2 & 2W (RP2350)

         .. code-block:: console

            bazelisk run //apps/blinky:rp2350_console

#. In ``pw_console`` click **File** then click **Insert Repl Snippet**.

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/snippet_v1.png

#. Select **Echo RPC** with your keyboard and then press :kbd:`Enter`.

   .. admonition:: Troubleshooting

      If clicking **Echo RPC** doesn't work, try the keyboard-based
      workflow.

   You should see the **Python Repl** input prompt (bottom-left pane) get auto-populated
   with an echo command.

#. Focus the **Python Repl** and then press :kbd:`Enter` to execute
   the pre-populated echo command.

   .. tip::

      If you're curious about how snippets are implemented, take a look
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
everything into a script.

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

   If the script fails: make sure that you closed the ``pw_console``
   instance from the last section before attempting this section.

You should see output similar to this:

.. code-block:: console

   20241221 08:20:18 INF Using serial port: /dev/ttyACM0
   20241221 08:20:18 DBG Starting read process
   20241221 08:20:18 DBG Starting PendingRpc(channel=1, method=pw.log.Logs.Listen, call_id=1)
   20241221 08:20:18 DBG Using selector: EpollSelector
   20241221 08:20:18 INF Calling Echo(msg="Hello")
   20241221 08:20:18 DBG Starting PendingRpc(channel=1, method=pw.rpc.EchoService.Echo, call_id=2)
   20241221 08:20:18 DBG PendingRpc(channel=1, method=pw.rpc.EchoService.Echo, call_id=2) received response: msg: "Hello"

   20241221 08:20:18 INF PendingRpc(channel=1, method=pw.rpc.EchoService.Echo, call_id=2) completed: Status.OK
   The status was Status.OK
   The message was Hello
   20241221 08:20:18 INF Calling Echo(msg="Goodbye!")
   20241221 08:20:18 DBG Starting PendingRpc(channel=1, method=pw.rpc.EchoService.Echo, call_id=3)
   20241221 08:20:18 DBG PendingRpc(channel=1, method=pw.rpc.EchoService.Echo, call_id=3) received response: msg: "Goodbye!"

   20241221 08:20:18 INF PendingRpc(channel=1, method=pw.rpc.EchoService.Echo, call_id=3) completed: Status.OK
   Status.OK: msg: "Goodbye!"

   20241221 08:20:18 DBG Stopping read process

Towards the end of the output you can see the echoed message and the
printed status information.

.. tip::

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
