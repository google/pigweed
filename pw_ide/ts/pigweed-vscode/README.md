# Pigweed Extension for Visual Studio Code

Work more effectively with [Pigweed](https://pigweed.dev) projects in Visual
Studio Code!

## Usage

Right now, all functionality is exposed through [tasks](https://code.visualstudio.com/docs/editor/tasks),
so your first step will be to open the [command palette](https://code.visualstudio.com/docs/getstarted/userinterface#_command-palette)
and select `Tasks: Run Task`.

### Sync your IDE to your project

Running the `Pigweed: Sync IDE` task is the equivalent of running the
[`pw ide sync`](https://pigweed.dev/pw_ide/#sync) CLI command.

### Manage C++ target toolchains

For more information, see [this page](https://pigweed.dev/docs/editors.html#c-c)
in the Pigweed docs.

Running the `Pigweed: Set C++ Target Toolchain` will display a list of available
target toolchains. Simply select one, and `clangd` will be configured to use it.

### Manage the Python virtual environment

Usually, `Pigweed: Sync IDE` will automatically configure Visual Studio Code to
use the correct Pigweed Python virtual environment. But if you need to change
this manually, you can do so with `Pigweed: Set Python Virtual Environment`.

### Other tasks

- `Pigweed: Format` == `pw format --fix`
- `Pigweed: Presubmit` == `pw presubmit`

### Found a bug?

Run `Pigweed: File Bug` to let us know!

## Configuration

This extension provides the following configuration options:

- `pigweed.enforceExtensionRecommendations`: If set to `true`, recommendations
  in your project's `extensions.json` will be enforced. Recommended extensions
  will need to be installed, and non-recommended extensions will need to be
  disabled in the project's workspace.

## Developing

If you want to contribute to this extension, here's how you can get started:

- Ensure that you have `npm` installed globally; this doesn't use the
  distribution provided by Pigweed yet.

- Open the `pigweed/pw_ide/ts/pigweed-vscode` directory directly in Visual
  Studio Code.

- Run `npm install` to add all dependencies.

- Run "Run Extension" in the "Run and Debug" sidebar, or simply hit F5. A new
  Visual Studio Code window will open with the extension installed.

- Make changes. The build will update automatically. Click the little green
  circle-with-an-arrow icon at the top of your development window to update
  the extension development host with the new build.

- Run tests: `npm run test`

### Building

- Install the build tool: `npm install -g @vscode/vsce`

- Run `pw ide vscode --build-extension`
