.. _showcase-sense-tutorial-intel:

================================
4. Explore C++ code intelligence
================================
Sense's integration with :ref:`module-pw_ide` enables fast and accurate
code navigation, autocompletion based on a deep understanding of the
code structure, and instant compiler warnings and errors. Try the
code navigation now:

.. _Command Palette: https://code.visualstudio.com/docs/getstarted/userinterface#_command-palette

.. tab-set::

   .. tab-item:: VS Code
      :sync: vsc

      #. Press :kbd:`Control+Shift+P` (:kbd:`Command+Shift+P` on macOS) to open
         the `Command Palette`_.

      #. Start typing ``Pigweed: Select Code Analysis Target`` and press
         :kbd:`Enter` to start executing that command.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/target.png

         .. tip::

            You can also select code analysis targets from the bottom-left
            of your VS Code GUI, in the status bar. In the next image,
            the mouse cursor is hovering over the GUI element for selecting
            code analysis targets.

            .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/select_target_status_bar.png

      #. Select the ``rp2040`` option.

         .. tip::

            In the status bar (the bar at the bottom of VS Code) you may see
            informational messages about clang indexing. You don't need to wait
            for that indexing to complete.

         The code intelligence is now set up to help you with physical Pico
         programming. If you had selected the other option, ``host_simulator``,
         the code intelligence would be set up to help with programming the
         simulated app that you will run on your development host in
         :ref:`showcase-sense-tutorial-sim`. Verify the platform-specific
         code intelligence now by making sure that :ref:`module-pw_log` invocations
         resolve to different backends.

      #. Open ``//apps/blinky/main.cc``.

      #. Right-click the ``PW_LOG_INFO()`` invocation and select
         **Go to Definition**.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/go_to_definition.png
            :alt: Selecting "Go to Definition" after right-clicking PW_LOG_INFO()

         This should take you to a file called ``log.h``:

         .. code-block:: c++

            #ifndef PW_LOG_INFO
            #define PW_LOG_INFO(...) \  // Your cursor should be on this line.
              PW_LOG(PW_LOG_LEVEL_INFO, PW_LOG_MODULE_NAME, PW_LOG_FLAGS, __VA_ARGS__)
            #endif  // PW_LOG_INFO

      #. Right-click the ``PW_LOG()`` invocation (on the line below your cursor)
         and select **Go to Definition** again. You should jump here:

         .. code-block:: c++

            #ifndef PW_LOG
            #define PW_LOG(  // Your cursor should be on this line.
              // ...
                  PW_HANDLE_LOG(level, module, flags, __VA_ARGS__);
              // ...


      #. Finally, right-click the ``PW_HANDLE_LOG()`` invocation
         and select **Go to Definition** one last time. You should jump to
         a :ref:`module-pw_log_tokenized` backend header. You can hover
         over the filename in the tab (``log_backend.h``) to see the full
         path to the header.

      #. Open the Command Palette, switch your target to ``host_simulator``,
         and then repeat this **Go to Definition** workflow again, starting from
         ``//apps/blinky/main.cc``. You should see the definitions finally
         resolve to a :ref:`module-pw_log_string` backend header.

         This proves that code intelligence is working because the original
         call to ``PW_LOG_INFO()`` in ``//apps/blinky/main.cc`` is basically
         a generic API that gets resolved at compile-time. The resolution
         depends on what platform you're building for (``rp2040`` or
         ``host_simulator``). See :ref:`docs-facades`.

      Here's a diagram summary of how the code intelligence resolved to
      different files depending on the code analysis target you selected:

      .. mermaid::

         flowchart LR

           a["main.cc"] --> b["log.h"]
           b["log.h"] -. rp2040 .-> c["pw_log_tokenized/.../log_backend.h"]
           b["log.h"] -. host_simulator .-> d["pw_log_string/.../log_backend.h"]

   .. tab-item:: CLI
      :sync: cli

      This feature is only supported in VS Code.

.. _showcase-sense-tutorial-intel-summary:

-------
Summary
-------
Portable, hardware-agnostic software abstractions such as :ref:`module-pw_log`
make it easier to reuse code across projects and hardware platforms. But they
also make it more difficult to correctly navigate references in your codebase.
The Pigweed extension for VS Code can solve this problem; you just need to
tell it what hardware platform within your codebase it should focus on.

Next, head over to :ref:`showcase-sense-tutorial-hosttests` to learn how to run
unit tests on your development host so that you can verify that Sense's logic is
correct before attempting to run it.
