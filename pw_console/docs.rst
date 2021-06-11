.. _module-pw_console:

----------
pw_console
----------

The Pigweed Console provides a Python repl (read eval print loop) using
`ptpython <https://github.com/prompt-toolkit/ptpython>`_ and a log message
viewer in a single-window terminal based interface. It is designed to be a
replacement for
`IPython's embed() <https://ipython.readthedocs.io/en/stable/interactive/reference.html#embedding>`_
function.

==========
Motivation
==========

``pw_console`` is the complete solution for interacting with hardware devices
using :ref:`module-pw_rpc` over a :ref:`module-pw_hdlc` transport.

The repl allows interactive RPC sending while the log viewer provides immediate
feedback on device status.

=====
Usage
=====

``pw console`` is invoked by calling the ``embed()`` function in your own
Python script.

.. automodule:: pw_console.console_app
    :members: embed
    :undoc-members:
    :show-inheritance:

=========================
Implementation and Design
=========================

Detains on Pigweed Console internals follows.

Thread and Event Loop Design
----------------------------

Here's a diagram showing how ``pw_console`` threads and asyncio tasks are organized.

.. mermaid::

   flowchart LR
       classDef eventLoop fill:#e3f2fd,stroke:#90caf9,stroke-width:1px;
       classDef thread fill:#fffde7,stroke:#ffeb3b,stroke-width:1px;
       classDef plugin fill:#fce4ec,stroke:#f06292,stroke-width:1px;
       classDef builtinFeature fill:#e0f2f1,stroke:#4db6ac,stroke-width:1px;

       %% Subgraphs are drawn in reverse order.

       subgraph pluginThread [Plugin Thread 1]
           subgraph pluginLoop [Plugin Event Loop 1]
               toolbarFunc-->|"Refresh<br/>UI Tokens"| toolbarFunc
               toolbarFunc[Toolbar Update Function]
           end
           class pluginLoop eventLoop;
       end
       class pluginThread thread;

       subgraph pluginThread2 [Plugin Thread 2]
           subgraph pluginLoop2 [Plugin Event Loop 2]
               paneFunc-->|"Refresh<br/>UI Tokens"| paneFunc
               paneFunc[Pane Update Function]
           end
           class pluginLoop2 eventLoop;
       end
       class pluginThread2 thread;

       subgraph replThread [Repl Thread]
           subgraph replLoop [Repl Event Loop]
               Task1 -->|Finished| Task2 -->|Cancel with Ctrl-C| Task3
           end
           class replLoop eventLoop;
       end
       class replThread thread;

       subgraph main [Main Thread]
           subgraph mainLoop [User Interface Event Loop]
               log[[Log Pane]]
               repl[[Python Repl]]
               pluginToolbar([User Toolbar Plugin])
               pluginPane([User Pane Plugin])
               class log,repl builtinFeature;
               class pluginToolbar,pluginPane plugin;
           end
           class mainLoop eventLoop;
       end
       class main thread;

       repl-.->|Run Code| replThread
       pluginToolbar-.->|Register Plugin| pluginThread
       pluginPane-.->|Register Plugin| pluginThread2
