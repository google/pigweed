# Change Log

## [0.1.1]

- Fixes cases where "Pigweed: Check Extensions" was not running on startup.

## [0.1.0]

- Adds the "Pigweed: Check Extensions" command, which prompts the user to
  install all recommended extensions and disable all unwanted extensions, as
  defined by the project's `extensions.json`. This makes "recommended"
  extensions required, and "unwanted" extensions forbidden, allowing Pigweed
  projects to define more consistent development environments for their teams.
