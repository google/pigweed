# Change Log

## [0.1.3]

Minor bugfix for `Pigweeed: File Bug`.

## [0.1.2]

- You can now submit bugs to the [Pigweed issue tracker](https://issues.pigweed.dev/issues?q=status:open)
  directly from your editor. Just run `Pigweed: File Bug`.
- Several UX improvements, including:
  - Auto-detection and activation in Pigweed projects is more reliable
  - Clearer feedback on the need to install or disable extensions
  - Target toolchains are now presented in alphabetical order
  - Errors will not appear when working in Pigweed projects that don't define
    project-level extensions

## [0.1.1]

- Fixes cases where "Pigweed: Check Extensions" was not running on startup.

## [0.1.0]

- Adds the "Pigweed: Check Extensions" command, which prompts the user to
  install all recommended extensions and disable all unwanted extensions, as
  defined by the project's `extensions.json`. This makes "recommended"
  extensions required, and "unwanted" extensions forbidden, allowing Pigweed
  projects to define more consistent development environments for their teams.
