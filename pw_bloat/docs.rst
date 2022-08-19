.. _module-pw_bloat:

--------
pw_bloat
--------
The bloat module provides tools and helpers around using
`Bloaty McBloatface <https://github.com/google/bloaty>`_ including generating
generate size report cards for output binaries through Pigweed's GN build
system.

Bloat report cards allow tracking the memory usage of a system over time as code
changes are made and provide a breakdown of which parts of the code have the
largest size impact.

.. _bloat-howto:

Defining size reports
=====================
Size reports are defined using the GN template ``pw_size_diff``. The template
requires at least two executable targets on which to perform a size diff. The
base for the size diff can be specified either globally through the top-level
``base`` argument, or individually per-binary within the ``binaries`` list.

**Arguments**

* ``title``: Title for the report card.
* ``base``: Optional default base target for all listed binaries.
* ``binaries``: List of binaries to size diff. Each binary specifies a target,
  a label for the diff, and optionally a base target that overrides the default
  base.
* ``source_filter``: Optional regex to filter labels in the diff output.
* ``full_report``: Boolean flag indicating whether to output a full report of
  all symbols in the binary, or a summary of the segment size changes. Default
  false.

.. code::

  import("$dir_pw_bloat/bloat.gni")

  executable("empty_base") {
    sources = [ "empty_main.cc" ]
  }

  executable("hello_world_printf") {
    sources = [ "hello_printf.cc" ]
  }

  executable("hello_world_iostream") {
    sources = [ "hello_iostream.cc" ]
  }

  pw_size_diff("my_size_report") {
    title = "Hello world program using printf vs. iostream"
    base = ":empty_base"
    binaries = [
      {
        target = ":hello_world_printf"
        label = "Hello world using printf"
      },
      {
        target = ":hello_world_iostream"
        label = "Hello world using iostream"
      },
    ]
  }

Single Binary Sizes Reports
^^^^^^^^^^^^^^^^^^^^^^^^^^^
**Argument**

* ``target``: Binary target to run size report on.

.. code::

  import("$dir_pw_bloat/bloat.gni")

  executable("hello_world_iostream") {
    sources = [ "hello_iostream.cc" ]
  }

  pw_size_report("hello_world_iostream_size_report") {
    target = ":hello_iostream"
  }

ASCII Table Generated

.. code-block:: rst

  +-------------+-----------------------------------------------+------+
  |segment_names|                  fullsymbols                  | sizes|
  +=============+===============================================+======+
  |FLASH        |                                               |20,416|
  |             |[147 Others]                                   | 6,976|
  |             |_dtoa_r                                        | 3,036|
  |             |[section .code]                                | 2,985|
  |             |_printf_float                                  | 1,132|
  |             |__adddf3                                       |   630|
  |             |_vfiprintf_r                                   |   608|
  |             |__aeabi_dmul                                   |   596|
  |             |_printf_i                                      |   588|
  |             |_svfprintf_r                                   |   512|
  |             |__aeabi_ddiv                                   |   464|
  |             |pw_assert_basic_HandleFailure                  |   388|
  |             |__multiply                                     |   340|
  |             |quorem                                         |   278|
  |             |__mdiff                                        |   276|
  |             |__sflush_r                                     |   268|
  |             |_ctype_                                        |   257|
  |             |__lshift                                       |   224|
  |             |__swsetup_r                                    |   220|
  |             |pw::allocator::FreeListHeap::Free(void*)       |   220|
  |             |_printf_common                                 |   218|
  |             |__mprec_tens                                   |   200|
  +-------------+-----------------------------------------------+------+
  |RAM          |                                               |   672|
  |             |__global_locale                                |   364|
  |             |(anonymous namespace)::buf                     |   104|
  |             |impure_data                                    |    96|
  |             |kCrashBanner                                   |    48|
  |             |object.0                                       |    24|
  |             |[section .zero_init_ram]                       |     9|
  |             |[section .static_init_ram]                     |     4|
  |             |_impure_ptr                                    |     4|
  |             |errno                                          |     4|
  |             |pw::log_basic::(anonymous namespace)::write_log|     4|
  |             |pw_freelist_heap                               |     4|
  |             |unoptimizable                                  |     4|
  |             |__lock___sfp_recursive_mutex                   |     1|
  |             |__lock___sinit_recursive_mutex                 |     1|
  +-------------+-----------------------------------------------+------+
  |Total        |                                               |21,088|
  +-------------+-----------------------------------------------+------+


Size reports are typically included in ReST documentation, as described in
`Documentation integration`_. Size reports may also be printed in the build
output if desired. To enable this in the GN build, set the
``pw_bloat_SHOW_SIZE_REPORTS`` build arg to ``true``.

Documentation integration
=========================
Bloat reports are easy to add to documentation files. All ``pw_size_diff``
targets output a file containing a tabular report card. This file can be
imported directly into a ReST documentation file using the ``include``
directive.

For example, the ``simple_bloat_loop`` and ``simple_bloat_function`` size
reports under ``//pw_bloat/examples`` are imported into this file as follows:

.. code:: rst

  Simple bloat loop example
  ^^^^^^^^^^^^^^^^^^^^^^^^^
  .. include:: examples/simple_bloat_loop

  Simple bloat function example
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  .. include:: examples/simple_bloat_function

Resulting in this output:

Simple bloat loop example
^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: examples/simple_bloat_loop

Simple bloat function example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: examples/simple_bloat_function

Additional Bloaty data sources
==============================
`Bloaty McBloatface <https://github.com/google/bloaty>`_ by itself cannot help
answer some questions which embedded developers frequently face such as
understanding how much space is left. To address this, Pigweed provides Python
tooling (``pw_bloat.bloaty_config``) to generate bloaty configuration files
based on the final ELF files through small tweaks in the linker scripts to
expose extra information.

See the sections below on how to enable the additional data sections through
modifications in your linker script(s).

As an example to generate the helper configuration which enables additional data
sources for ``example.elf`` if you've updated your linker script(s) accordingly,
simply run
``python -m pw_bloaty.bloaty_config example.elf > example.bloaty``. The
``example.bloaty``  can then be used with bloaty using the ``-c`` flag, for
example
``bloaty -c example.bloaty example.elf --domain vm -d memoryregions,utilization``
which may return something like:

.. code-block::

    84.2%  1023Ki    FLASH
      94.2%   963Ki    Free space
       5.8%  59.6Ki    Used space
    15.8%   192Ki    RAM
     100.0%   192Ki    Used space
     0.0%     512    VECTOR_TABLE
      96.9%     496    Free space
       3.1%      16    Used space
     0.0%       0    Not resident in memory
       NAN%       0    Used space


``utilization`` data source
^^^^^^^^^^^^^^^^^^^^^^^^^^^
The most common question many embedded developers face when using ``bloaty`` is
how much space you are using and how much space is left. To correctly answer
this, section sizes must be used in order to correctly account for section
alignment requirements.

The generated ``utilization`` data source will work with any ELF file, where
``Used Space`` is reported for the sum of virtual memory size of all sections.

In order for ``Free Space`` to be reported, your linker scripts must include
properly aligned sections which span the unused remaining space for the relevant
memory region with the ``unused_space`` string anywhere in their name. This
typically means creating a trailing section which is pinned to span to the end
of the memory region.

For example imagine this partial example GNU LD linker script:

.. code-block::

  MEMORY
  {
    FLASH(rx) : \
      ORIGIN = PW_BOOT_FLASH_BEGIN, \
      LENGTH = PW_BOOT_FLASH_SIZE
    RAM(rwx) : \
      ORIGIN = PW_BOOT_RAM_BEGIN, \
      LENGTH = PW_BOOT_RAM_SIZE
  }

  SECTIONS
  {
    /* Main executable code. */
    .code : ALIGN(4)
    {
      /* Application code. */
      *(.text)
      *(.text*)
      KEEP(*(.init))
      KEEP(*(.fini))

      . = ALIGN(4);
      /* Constants.*/
      *(.rodata)
      *(.rodata*)
    } >FLASH

    /* Explicitly initialized global and static data. (.data)*/
    .static_init_ram : ALIGN(4)
    {
      *(.data)
      *(.data*)
      . = ALIGN(4);
    } >RAM AT> FLASH

    /* Zero initialized global/static data. (.bss) */
    .zero_init_ram (NOLOAD) : ALIGN(4)
    {
      *(.bss)
      *(.bss*)
      *(COMMON)
      . = ALIGN(4);
    } >RAM
  }

Could be modified as follows enable ``Free Space`` reporting:

.. code-block::

  MEMORY
  {
    FLASH(rx) : ORIGIN = PW_BOOT_FLASH_BEGIN, LENGTH = PW_BOOT_FLASH_SIZE
    RAM(rwx) : ORIGIN = PW_BOOT_RAM_BEGIN, LENGTH = PW_BOOT_RAM_SIZE

    /* Each memory region above has an associated .*.unused_space section that
     * overlays the unused space at the end of the memory segment. These
     * segments are used by pw_bloat.bloaty_config to create the utilization
     * data source for bloaty size reports.
     *
     * These sections MUST be located immediately after the last section that is
     * placed in the respective memory region or lld will issue a warning like:
     *
     *   warning: ignoring memory region assignment for non-allocatable section
     *      '.VECTOR_TABLE.unused_space'
     *
     * If this warning occurs, it's also likely that LLD will have created quite
     * large padded regions in the ELF file due to bad cursor operations. This
     * can cause ELF files to balloon from hundreds of kilobytes to hundreds of
     * megabytes.
     *
     * Attempting to add sections to the memory region AFTER the unused_space
     * section will cause the region to overflow.
     */
  }

  SECTIONS
  {
    /* Main executable code. */
    .code : ALIGN(4)
    {
      /* Application code. */
      *(.text)
      *(.text*)
      KEEP(*(.init))
      KEEP(*(.fini))

      . = ALIGN(4);
      /* Constants.*/
      *(.rodata)
      *(.rodata*)
    } >FLASH

    /* Explicitly initialized global and static data. (.data)*/
    .static_init_ram : ALIGN(4)
    {
      *(.data)
      *(.data*)
      . = ALIGN(4);
    } >RAM AT> FLASH

    /* Represents unused space in the FLASH segment. This MUST be the last
     * section assigned to the FLASH region.
     */
    .FLASH.unused_space (NOLOAD) : ALIGN(4)
    {
      . = ABSOLUTE(ORIGIN(FLASH) + LENGTH(FLASH));
    } >FLASH

    /* Zero initialized global/static data. (.bss). */
    .zero_init_ram (NOLOAD) : ALIGN(4)
    {
      *(.bss)
      *(.bss*)
      *(COMMON)
      . = ALIGN(4);
    } >RAM

    /* Represents unused space in the RAM segment. This MUST be the last section
     * assigned to the RAM region.
     */
    .RAM.unused_space (NOLOAD) : ALIGN(4)
    {
      . = ABSOLUTE(ORIGIN(RAM) + LENGTH(RAM));
    } >RAM
  }

``memoryregions`` data source
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Understanding how symbols, sections, and other data sources can be attributed
back to the memory regions defined in your linker script is another common
problem area. Unfortunately the ELF format does not include the original memory
regions, meaning ``bloaty`` can not do this today by itself. In addition, it's
relatively common that there are multiple memory regions which alias to the same
memory but through different buses which could make attribution difficult.

Instead of taking the less portable and brittle approach to parse ``*.map``
files, ``pw_bloat.bloaty_config`` consumes symbols which are defined in the
linker script with a special format to extract this information from the ELF
file: ``pw_bloat_config_memory_region_NAME_{start,end}{_N,}``.

These symbols are then used to determine how to map segments to these memory
regions. Note that segments must be used in order to account for inter-section
padding which are not attributed against any sections.

As an example, if you have a single view in the single memory region named
``FLASH``, then you should produce the following two symbols in your linker
script:

.. code-block::

  pw_bloat_config_memory_region_FLASH_start = ORIGIN(FLASH);
  pw_bloat_config_memory_region_FLASH_end = ORIGIN(FLASH) + LENGTH(FLASH);

As another example, if you have two aliased memory regions (``DCTM`` and
``ITCM``) into the same effective memory named you'd like to call ``RAM``, then
you should produce the following four symbols in your linker script:

.. code-block::

  pw_bloat_config_memory_region_RAM_start_0 = ORIGIN(ITCM);
  pw_bloat_config_memory_region_RAM_end_0 = ORIGIN(ITCM) + LENGTH(ITCM);
  pw_bloat_config_memory_region_RAM_start_1 = ORIGIN(DTCM);
  pw_bloat_config_memory_region_RAM_end_1 = ORIGIN(DTCM) + LENGTH(DTCM);
