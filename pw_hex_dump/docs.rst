.. _module-pw_hex_dump:

===========
pw_hex_dump
===========
.. pigweed-module::
   :name: pw_hex_dump

Sometimes on embedded systems there's a desire to view memory contents when
debugging various issues. While in some cases this can be done by attaching an
in-circuit debugger of some kind, form-factor hardware might not have this as an
option due to size constraints. Additionally, there's often quite a bit more
setup involved than simply adding a print statement.

A common practice to address this is setting up print statements that dump data
as logs when a certain event occurs. There's often value to formatting these
dumps as human readable key-value pairs, but sometimes there's a need to see the
raw binary data in different ways. This can help validate in memory/on flash
binary structure of stored data, among other things.

``pw_hex_dump`` is a handy toolbox that provides utilities to help dump data as
hex to debug issues. Unless otherwise specified, avoid depending directly on the
formatting of the output as it may change (unless otherwise specified). With
that said, the ``FormattedHexDumper`` strives to be xxd compatible by default.
