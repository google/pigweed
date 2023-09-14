# Pigweed Extension for Visual Studio Code

This is highly experimental!

## Developing

- Ensure that you have `npm` installed globally; this doesn't use the
  distribution provided by Pigweed yet.

- Open the `pigweed/pw_ide/vscode` directory directly in Visual Studio Code.

- Run `npm install` to add all dependencies.

- Run "Run Extension" in the "Run and Debug" sidebar, or simply hit F5. A new
  Visual Studio Code window will open with the extension installed.

- Make changes. The build will update automatically. Click the little green
  circle-with-an-arrow icon at the top of your development window to update
  the extension development host with the new build.

## Building

- Install the build tool: `npm install -g @vscode/vsce`

- Build the VSIX: `vsce package`

## Changelog

### 0.0.1

- Adds the "Pigweed: Check Extensions" command, which prompts the user to
  install all recommended extensions and disable all unwanted extensions, as
  defined by the project's `extensions.json`.
