.. _module-pw_protobuf-decoding:

--------
Decoding
--------

Size report
===========

Full size report
^^^^^^^^^^^^^^^^

This report demonstrates the size of using the entire decoder with all of its
decode methods and a decode callback for a proto message containing each of the
protobuf field types.

.. include:: size_report/decoder_full


Incremental size report
^^^^^^^^^^^^^^^^^^^^^^^

This report is generated using the full report as a base and adding some int32
fields to the decode callback to demonstrate the incremental cost of decoding
fields in a message.

.. include:: size_report/decoder_incremental
