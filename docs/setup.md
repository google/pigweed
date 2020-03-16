# Setup

Pigweed uses a combination of CIPD and Python virtual environments to provide
you with the tools necessary for Pigweed development without modifying your
system's development environment. This guide will walk you through the steps
needed to download and set up the Pigweed repository.

We hope to make the setup process as smooth as possible. If any of this doesn't
work, please [file a bug](https://bugs.chromium.org/p/pigweed/issues/entry).

## Prerequisites

**Linux**<br/>
Linux should work out of box, and not require any manual installation of
prerequisites.

**macOS**<br/>
macOS does not require any prerequisites to be installed, but you may run into
some Mac-specific configuration issues when you begin the bootstrap process.

If you're using Homebrew and you get an error saying
`module 'http.client' has no attribute 'HTTPSConnection'` then your
Homebrew Python was not set up to support SSL. Ensure it's installed with
`brew install openssl` and then run
`brew uninstall python && brew install python`.

To flash firmware to a STM32 Discovery development board (and run `pw test`)
from macOS, you will need to install OpenOCD. Install Homebrew using the latest
instructions at https://brew.sh/, then install OpenOCD with
`brew install openocd`.

**Windows**<br/>
To start using Pigweed on Windows, you'll need to install Git and Python (2.7 or
above). We recommend you install Git to run from the command line and third
party software.

## Setup

Once you have prerequisites satisfied, you should be able to clone Pigweed and
run the bootstrap that initializes the Pigweed virtual environment.

**Linux/macOS**
```bash
$ git clone sso://pigweed.googlesource.com/pigweed/pigweed ~/pigweed
$ cd ~/pigweed
$ . bootstrap.sh
```

**Windows**
```batch
:: Run git commands from the shell you set up to use with Git during install.
> cd %HOMEPATH%\pigweed
> git clone sso://pigweed.googlesource.com/pigweed/pigweed
> bootstrap.bat
```

## Contributors

If you plan to contribute to Pigweed, you'll need to set up a commit hook for
Gerrit.

**Linux/macOS**<br/>
```bash
$ f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f
```

**Windows**<br/>
Download [the Gerrit commit hook](https://gerrit-review.googlesource.com/tools/hooks/commit-msg)
and then copy it to the `.git\hooks` directory in the Pigweed repository.
```batch
copy %HOMEPATH%\Downloads\commit-msg %HOMEPATH%\pigweed\.git\hooks\commit-msg
```

All Pigweed CLs must adhere to Pigweed's style guide and pass automated builds,
tests, and style checks to be merged upstream. Much of this checking is done
using Pigweed's pw_presubmit module. To speed up the review process, consider
adding `pw presubmit` as a git push hook using the following command:

**Linux/macOS**<br/>
```bash
$ pw presubmit --install
```

## Congratulations!
You should now be set up to start using Pigweed. If you are interested in seeing
some of what Pigweed can do, check out the [developer guide](developer_guide.md).
