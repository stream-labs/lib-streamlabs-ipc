# Node.JS Streamlabs IPC Sample
This is a Node.JS example with a electron test under /electron/.

# Requirements
You need to have the following installed:

- Visual Studio 2013, 2015 or 2017
- CMake
- Node.JS with NPM

# Building
## Setup
1. Install cmake-js: `npm -g install cmake-js`

## Building
1. Open shell in the source directory.
2. `cmake-js build --runtime electron --runtime-version 1.7.7`
3. `cmake --build build --target INSTALL`
4. Pick up built binaries from /distrib/

# Electron Sample
1. Copy the freshly built binary directory to /electron/ and rename it streamlabs-ipc-node.
2. Start the server with the correct path (see index.html).
3. Start the electron app by running `electron .` in the /electron/ directory.
