# Log Viewer

An embeddable log-viewing web component that is customizable and extensible for use in various developer contexts.

Visit [go/fxd-pigweed-log-viewer](http://goto.google.com/fxd-pigweed-log-viewer) for more information.

## Installation

1. Clone the main Pigweed repository:

```
git clone https://pigweed.googlesource.com/pigweed/pigweed
```

2. Navigate to the project directory:

```
cd pigweed
```

3. Install the necessary dependencies:

```
source bootstrap.sh
```

## Build

1. Navigate to the project directory:

```
cd pigweed/pigweed_web/log-viewer
```


2. run the following command at the root of the project:

```
npm run build
```

This will generate the compiled files used in the application.

## Development Mode

To run the application in development mode, use the following command:

```
npm run dev -- --host
```

This will start the development server and launch the application. The `--host` flag is optional and can be used to specify the host address.

## Usage