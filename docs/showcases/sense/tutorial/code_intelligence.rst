.. _showcase-sense-tutorial-intel:

================================
4. Explore C++ code intelligence
================================
Sense's integration with :ref:`module-pw_ide` enables fast and accurate
code navigation, autocompletion based on a deep understanding of the
code structure, and instant compiler warnings and errors. Try intelligent
code navigation features now:

.. _showcase-sense-tutorial-intel-nav:

-------------------------------
Navigate the code intelligently
-------------------------------
Imagine this hypothetical development scenario. Suppose that you need
to modify how your app logs messages. Your first task is to understand
how logging currently works on the Pico 1. You know that your app uses
:ref:`module-pw_log` and that ``pw_log`` uses a special type of API called
a :ref:`facade <docs-facades>` where the implementation can be changed during
compilation. The plan is to use the intelligent code navigation features of
``pw_ide`` to determine the true, final implementation of the logging functions
used in your project.

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
            the mouse cursor (bottom-left of image) is hovering over the GUI element
            for selecting code analysis targets.

            .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/select_target_status_bar.png

      #. Select the ``rp2040`` option.

         The code intelligence is now set up to help you with physical Pico 1
         programming. The RP2040 is the microprocessor that powers the Pico 1.
         If you had selected the other option, ``host_simulator``,
         the code intelligence would be set up to help with programming the
         simulated app that you will run on your development host later.
         Code intelligence for the Pico 2 (``rp2350``) isn't supported yet.
         ``rp2040`` represents one platform, ``host_simulator`` another.

         We will verify the platform-specific code intelligence now by making sure
         that :ref:`module-pw_log` invocations resolve to different backends. This
         will make more sense after you do the hands-on demonstration.

      #. Open ``//apps/blinky/main.cc``.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/blinky_main_v1.png

      #. Right-click the ``PW_LOG_INFO()`` invocation and select
         **Go to Definition**.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/go_to_definition.png
            :alt: Selecting "Go to Definition" after right-clicking PW_LOG_INFO()

         This should take you to a file called ``log.h`` Your cursor should be
         on the line that defines ``PW_LOG_INFO``:

         .. code-block:: c++
            :emphasize-lines: 2

            #ifndef PW_LOG_INFO
            #define PW_LOG_INFO(...) \  // Your cursor should be on this line.
              PW_LOG(PW_LOG_LEVEL_INFO, PW_LOG_MODULE_NAME, PW_LOG_FLAGS, __VA_ARGS__)
            #endif  // PW_LOG_INFO

      #. Right-click the ``PW_LOG()`` invocation (on the line below your cursor)
         and select **Go to Definition** again.

         Your cursor should jump to the line that defines ``PW_LOG``:

         .. code-block:: c++
            :emphasize-lines: 2

            // Your cursor should be on the next line.
            #define PW_LOG(                                                            \
                level, verbosity, module, flags, /* format string and arguments */...) \
              do {                                                                     \
                if (PW_LOG_ENABLE_IF(level, verbosity, module, flags)) {               \
                  PW_HANDLE_LOG(level, module, flags, __VA_ARGS__);                    \
                }                                                                      \
              } while (0)

      #. Finally, right-click the ``PW_HANDLE_LOG()`` invocation
         and select **Go to Definition** one last time. You should jump to
         a :ref:`module-pw_log_tokenized` backend header. You can hover
         over the filename in the tab (``log_backend.h``) to see the full
         path to the header.

         .. code-block: c++

            #define PW_HANDLE_LOG PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD

      #. Open the Command Palette, switch your target to ``host_simulator``,
         and then repeat this workflow again, starting from the ``PW_LOG_INFO``
         invocation in ``//apps/blinky/main.cc``. You should see the definitions finally
         resolve to a :ref:`module-pw_log_string` backend header.

   .. tab-item:: CLI
      :sync: cli

      This feature is only supported in VS Code.

.. _showcase-sense-tutorial-intel-explanation:

-----------
Explanation
-----------
When you set your platform to ``rp2040`` and followed the call to
``PW_LOG_INFO()`` in ``//apps/blinky/main.cc`` back to its source,
you ended on a header within ``pw_log_tokenized``. When you repeated
the process a second time with the ``host_simulator`` platform you
ended on a header in a different module, ``pw_log_string``. This proves
that intelligent code navigation is working. The ``pw_log`` API is a
:ref:`facade <docs-facades>`. It's implementation is swapped out during
compilation depending on what platform you're building for. The ``rp2040``
platform has been set up to use the ``pw_log_tokenized`` implementation, whereas
the ``host_simulator`` platform uses the ``pw_log_string`` implementation.

Here's a diagram summary of how the intelligent code navigation resolved to
different files depending on the code analysis target you selected:

.. mermaid::

   flowchart LR

     a["main.cc"] --> b["log.h"]
     b["log.h"] -. rp2040 .-> c["pw_log_tokenized/.../log_backend.h"]
     b["log.h"] -. host_simulator .-> d["pw_log_string/.../log_backend.h"]

.. _showcase-sense-tutorial-intel-summary:

-------
Summary
-------
Portable, hardware-agnostic software abstractions such as :ref:`module-pw_log`
make it easier to reuse code across projects and hardware platforms. But they
also make it more difficult to correctly navigate references in your codebase.
The Pigweed extension for VS Code can solve this problem; you just need to
tell it what hardware platform within your codebase to focus on.

Next, head over to :ref:`showcase-sense-tutorial-hosttests` to learn how to run
unit tests on your development host.
