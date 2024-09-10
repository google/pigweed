Currently, the Fuchsia Bazel SDK requires all dependencies to define a `license` target from the `@rules_license` repository. Some of the dependencies for this project don't yet do that, which causes the build to fail.

These static license files will be used until either the downstream dependencies are updated or the Fuchsia Bazel SDK becomes more flexible to support both license definition types. More info in b/365825622.
