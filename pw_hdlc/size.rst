.. _module-pw_hdlc-size:

.. rst-class:: with-subtitle

==================
Code size analysis
==================
.. pigweed-module-subpage::
   :name: pw_hdlc

``pw_hdlc`` currently optimizes for robustness and flexibility instead of
binary size or performance.

The ``HDLC encode and decode`` size report shows the cost of everything needed
to use ``pw_hdlc``, including the dependencies on common modules like CRC32
from :ref:`module-pw_checksum` and variable-length integer handling from
:ref:`module-pw_varint`.

The ``HDLC encode and decode, ignoring CRC and varint`` size report shows the
cost of ``pw_hdlc`` if your application is already linking CRC32 and
variable-length integer handling. ``pw_varint`` is commonly used since it's
necessary for protocol buffer handling, so it's often already present.

.. include:: size_report

-----------------
More pw_hdlc docs
-----------------
.. include:: docs.rst
   :start-after: .. pw_hdlc-nav-start
   :end-before: .. pw_hdlc-nav-end
