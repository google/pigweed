.. _module-pw_hex_dump:

-----------
pw_hex_dump
-----------
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

DumpAddr()
==========
Dumps the value of a pointer (or size_t) as a hex string to a provided
destination buffer. While this sounds redundant to printf's %p or %zx, those
format specifiers are not universally available in all embedded libc
implementations. The goal is for this to be as portable as possible.

The output format for this function is expected to be stable.

FormattedHexDumper
==================
The formatted hex dumper is a configurable class that can dump hex in various
formats. The default produced output is xxd compatible, though there are options
to further adjust the output. One example is address prefixing, where base
memory address of each line is used instead of an offset.

Examples
--------

**Default:**

.. code-block:: none

   Offs. 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f  Text
   0000: a4 cc 32 62 9b 46 38 1a 23 1a 2a 7a bc e2 40 a0  ..2b.F8.#.*z..@.
   0010: ff 33 e5 2b 9e 9f 6b 3c be 9b 89 3c 7e 4a 7a 48  .3.+..k<...<~JzH
   0020: 18                                               .

**Example 1:**
(32-bit machine, group_every=4, prefix_mode=kAbsolute, bytes_per_line = 8)

.. code-block:: none

   Address     00       04        Text
   0x20000000: a4cc3262 9b46381a  ..2b.F8.
   0x20000008: 231a2a7a bce240a0  #.*z..@.
   0x20000010: ff33e52b 9e9f6b3c  .3.+..k<
   0x20000018: be9b893c 7e4a7a48  ...<~JzH
   0x20000020: 18                 .

**Example 2:**
(group_every=1, bytes_per_line = 16)

.. code-block:: none

   Offs. 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
   0000: a4 cc 32 62 9b 46 38 1a 23 1a 2a 7a bc e2 40 a0
   0010: ff 33 e5 2b 9e 9f 6b 3c be 9b 89 3c 7e 4a 7a 48
   0020: 18

**Example 3:**
(group_every=0, prefix_mode=kNone, show_header=false, show_ascii=false)

.. code-block:: none

   a4cc32629b46381a231a2a7abce240a0
   ff33e52b9e9f6b3cbe9b893c7e4a7a48
   18


Usage
-----
Here's an example of how this class might be used:

.. code-block:: cpp

   std::array<char, 80> temp;
   FormattedHexDumper hex_dumper(temp);
   hex_dumper.flags.show_ascii = false;
   hex_dumper.BeginDump(my_data);
   while(hex_dumper.DumpLine().ok()) {
     LOG_INFO("%s", temp.data());
   }

Which prints:

.. code-block:: none

   Offs. 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
   0000: a4 cc 32 62 9b 46 38 1a 23 1a 2a 7a bc e2 40 a0
   0010: ff 33 e5 2b 9e 9f 6b 3c be 9b 89 3c 7e 4a 7a 48
   0020: 18

Dependencies
============
* pw_bytes
* pw_span
* pw_status
