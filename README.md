# ilp_gaffer_movie
MovieReader/Writer nodes for Gaffer using ```libav``` (famously being the back-end for FFmpeg).

## Developer Notes

The simplest way to build the plug-ins is to use the provided Dockerfile as a [dev container](https://code.visualstudio.com/docs/devcontainers/containers). The docker image includes all the required dependencies ([Gaffer](https://github.com/GafferHQ/gaffer) and [FFmpeg](https://github.com/FFmpeg/FFmpeg)).

This project uses [CMake presets](https://devblogs.microsoft.com/cppblog/cmake-presets-integration-in-visual-studio-and-visual-studio-code/). To build the plug-ins you can use one of the provided configure presets ```gcc|clang Debug|Release```, specifying compiler and build type, respectively. If you want to use additional CMake options it is recommended to create a ```CMakeUserPresets.json``` file, where you can create a personal configure preset that inherits from one of the provided ones. That file should _not_ be part of the repository.

It is useful to set the following variables in the shell environment from which Gaffer is being launched while developing. This makes sure that Gaffer is able to load the freshly built plug-ins and that detailed log message are printed to the console.

```bash
# Change the path to wherever you install the plug-ins. 
export GAFFER_EXTENSION_PATHS=$repo/out/install/unixlike-gcc-debug:$GAFFER_EXTENSION_PATHS

# Print log messages from Gaffer.
export IECORE_LOG_LEVEL=Info
```