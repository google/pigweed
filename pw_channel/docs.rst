.. _module-pw_channel:

==========
pw_channel
==========
.. pigweed-module::
   :name: pw_channel

   ``pw_channel`` provides features that are essential for efficient,
   high-performance communications. The ``Channel`` API is:

   - **Flow-control-aware**: Built-in backpressure ensures that data is only
     requested when consumers are able to buffer and handle it.
   - **Zero-copy**: Data transfers seamlessly throughout the stack without
     copying between intermediate buffers or memory pools.
   - **Composable**: Layers of the communications stack are swappable, allowing
     more code reuse and configurability.
   - **Asynchronous**: No need for dedicated threads or nested callbacks.

Not sure if ``pw_channel`` is right for you? Check out
:ref:`module-pw_channel-design-why` to learn how ``pw_channel`` handles
flow control, backpressure, composability, and more.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Design
      :link: module-pw_channel-design
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How pw_channel handles:

      * Flow control
      * Backpressure
      * Composability
      * Asynchronous operations

      And more.

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: module-pw_channel-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      API reference for:

      * ``Channel``
      * ``AnyChannel``
      * ``ByteChannel``
      * ``DatagramChannel``

      And more.

.. toctree::
   :hidden:

   guides
   design
   reference
