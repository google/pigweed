.. _module-pw_toolchain_bazel-api:

=============
API reference
=============

.. py:class:: pw_cc_toolchain

  This rule is the core of a C/C++ toolchain definition. Critically, it is
  intended to fully specify the following:

  * Which tools to use for various compile/link actions.
  * Which `well-known features <https://bazel.build/docs/cc-toolchain-config-reference#wellknown-features>`_
    are supported.
  * Which flags to apply to various actions.

   .. py:attribute:: feature_deps
      :type: List[label]

      ``pw_cc_toolchain_feature`` labels that provide features for this toolchain.

   .. py:attribute:: ar
      :type: File

      Path to the tool to use for ``ar`` (static link) actions.

   .. py:attribute:: cpp
      :type: File

      Path to the tool to use for C++ compile actions.

   .. py:attribute:: gcc
      :type: File

      Path to the tool to use for C compile actions.

   .. py:attribute:: gcov
      :type: File

      Path to the tool to use for generating code coverage data.

   .. py:attribute:: ld
      :type: File

      Path to the tool to use for link actions.

   .. py:attribute:: strip
      :type: File

      Path to the tool to use for strip actions.

   .. py:attribute:: objcopy
      :type: File

      Path to the tool to use for objcopy actions.

   .. py:attribute:: objdump
      :type: File

      Path to the tool to use for objdump actions.

   .. py:attribute:: action_config_flag_sets
      :type: List[label]

      List of flag sets to apply to the respective ``action_config``\s. The vast
      majority of labels listed here will point to :py:class:`pw_cc_flag_set`
      rules.

   .. py:attribute:: toolchain_identifier
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: host_system_name
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: target_system_name
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: target_cpu
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: target_libc
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: compiler
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: abi_version
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: abi_libc_version
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

   .. py:attribute:: cc_target_os
      :type: str

      See  `cc_common.create_cc_toolchain_config_info() <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`_\.

.. py:class:: pw_cc_flag_set

   Declares an ordered set of flags bound to a set of actions.

   Flag sets can be attached to a :py:class:`pw_cc_toolchain` via
   :py:attr:`pw_cc_toolchain.action_config_flag_sets`\.

   Examples:

   .. code-block:: py

      pw_cc_flag_set(
          name = "warnings_as_errors",
          flags = ["-Werror"],
      )

      pw_cc_flag_set(
          name = "layering_check",
          flag_groups = [
              ":strict_module_headers",
              ":dependent_module_map_files",
          ],
      )

   .. inclusive-language: disable

   Note: In the vast majority of cases, alphabetical sorting is not desirable
   for the :py:attr:`pw_cc_flag_set.flags` and
   :py:attr:`pw_cc_flag_set.flag_groups` attributes.
   `Buildifier <https://github.com/bazelbuild/buildtools/blob/master/buildifier/README.md>`_
   shouldn't ever try to sort these, but in the off chance it starts to these
   members should be listed as exceptions in the ``SortableDenylist``.

   .. inclusive-language: enable

   .. py:attribute:: actions
      :type: List[str]

      A list of action names that this flag set applies to.

      .. inclusive-language: disable

      Valid choices are listed at
      `@rules_cc//cc:action_names.bzl <https://github.com/bazelbuild/bazel/blob/master/tools/build_defs/cc/action_names.bzl>`_\.

      .. inclusive-language: enable

      It is possible for some needed action names to not be enumerated in this list,
      so there is not rigid validation for these strings. Prefer using constants
      rather than manually typing action names.

   .. py:attribute:: flags
      :type: List[str]

      Flags that should be applied to the specified actions.

      These are evaluated in order, with earlier flags appearing earlier in the
      invocation of the underlying tool. If you need expansion logic, prefer
      enumerating flags in a :py:class:`pw_cc_flag_group` or create a custom
      rule that provides ``FlagGroupInfo``.

      Note: :py:attr:`pw_cc_flag_set.flags` and
      :py:attr:`pw_cc_flag_set.flag_groups` are mutually exclusive.

   .. py:attribute:: flag_groups
      :type: List[label]

      Labels pointing to :py:class:`pw_cc_flag_group` rules.

      This is intended to be compatible with any other rules that provide
      ``FlagGroupInfo``. These are evaluated in order, with earlier flag groups
      appearing earlier in the invocation of the underlying tool.

      Note: :py:attr:`pw_cc_flag_set.flag_groups` and
      :py:attr:`pw_cc_flag_set.flags` are mutually exclusive.

.. py:class:: pw_cc_flag_group

   Declares an (optionally parametric) ordered set of flags.

   :py:class:`pw_cc_flag_group` rules are expected to be consumed exclusively by
   :py:class:`pw_cc_flag_set` rules. Though simple lists of flags can be
   expressed by populating ``flags`` on a :py:class:`pw_cc_flag_set`,
   :py:class:`pw_cc_flag_group` provides additional power in the following two
   ways:

    1. Iteration and conditional expansion. Using
       :py:attr:`pw_cc_flag_group.iterate_over`,
       :py:attr:`pw_cc_flag_group.expand_if_available`\, and
       :py:attr:`pw_cc_flag_group.expand_if_not_available`\, more complex
       flag expressions can be made. This is critical for implementing things
       like the ``libraries_to_link`` feature, where library names are
       transformed into flags that end up in the final link invocation.

       Note: ``expand_if_equal``, ``expand_if_true``, and ``expand_if_false``
       are not yet supported.

    2. Flags are tool-independent. A :py:class:`pw_cc_flag_group` expresses
       ordered flags that may be reused across various
       :py:class:`pw_cc_flag_set` rules. This is useful for cases where multiple
       :py:class:`pw_cc_flag_set` rules must be created to implement a feature
       for which flags are slightly different depending on the action (e.g.
       compile vs link). Common flags can be expressed in a shared
       :py:class:`pw_cc_flag_group`, and the differences can be relegated to
       separate :py:class:`pw_cc_flag_group` instances.

   Examples:

   .. code-block:: py

      pw_cc_flag_group(
          name = "user_compile_flag_expansion",
          flags = ["%{user_compile_flags}"],
          iterate_over = "user_compile_flags",
          expand_if_available = "user_compile_flags",
      )

      # This flag_group might be referenced from various FDO-related
      # `pw_cc_flag_set` rules. More importantly, the flag sets pulling this in
      # may apply to different sets of actions.
      pw_cc_flag_group(
          name = "fdo_profile_correction",
          flags = ["-fprofile-correction"],
          expand_if_available = "fdo_profile_path",
      )

   .. py:attribute:: flags
      :type: List[str]

      List of flags provided by this rule.

      For extremely complex expressions of flags that require nested flag groups
      with multiple layers of expansion, prefer creating a custom rule in
      `Starlark <https://bazel.build/rules/language>`_ that provides
      ``FlagGroupInfo`` or ``FlagSetInfo``.


   .. py:attribute:: iterate_over
      :type: str

      Expands :py:attr:`pw_cc_flag_group.flags` for items in the named list.

      Toolchain actions have various variables accessible as names that can be
      used to guide flag expansions. For variables that are lists,
      :py:attr:`pw_cc_flag_group.iterate_over` must be used to expand the list into a series of flags.

      Note that :py:attr:`pw_cc_flag_group.iterate_over` is the string name of a
      build variable, and not an actual list. Valid options are listed in the
      `C++ Toolchain Configuration <https://bazel.build/docs/cc-toolchain-config-reference#cctoolchainconfiginfo-build-variables>`_
      reference.



      Note that the flag expansion stamps out the entire list of flags in
      :py:attr:`pw_cc_flag_group.flags` once for each item in the list.

      Example:

      .. code-block:: py

         # Expands each path in ``system_include_paths`` to a series of
         # ``-isystem`` includes.
         #
         # Example input:
         #     system_include_paths = ["/usr/local/include", "/usr/include"]
         #
         # Expected result:
         #     "-isystem /usr/local/include -isystem /usr/include"
         pw_cc_flag_group(
             name = "system_include_paths",
             flags = ["-isystem", "%{system_include_paths}"],
             iterate_over = "system_include_paths",
         )

   .. py:attribute:: expand_if_available
      :type: str

      Expands the expression in :py:attr:`pw_cc_flag_group.flags` if the
      specified build variable is set.

   .. py:attribute:: expand_if_not_available
      :type: str

      Expands the expression in :py:attr:`pw_cc_flag_group.flags` if the
      specified build variable is **NOT** set.
