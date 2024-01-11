# Gaffer Movie Nodes
At a high level, [Gaffer](https://github.com/GafferHQ/gaffer) can already read/write movies, since movies are essentially a sequence of images played back at a high enough frame rate to trick the human eye into interpreting the result as a continuous visual stream. To date, this is done by reading/writing image sequences in one of many popular formats (`.exr`, `.png`, or whatever [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) supports). As such movie frames are assumed to exist on disk as separate image files, and compression is thus only possible at a per-frame level. Now, while an image sequence might be suitable for a final delivery to a client, it is not always ideal to work with since data is spread across many (large-ish) files. Additionally, most (if not all) media players require image sequences to be encoded into a single movie file using one of many efficient video encoders (e.g. `ProRes`, `H264`, etc). Video encoders drastically reduce the size of an image sequence by performing temporal compression and many other tricks. The resulting pixel data is then stored in a bitstream that can be efficiently read from disk, which enables playback at the intended frame rate. 

Video encoders typically perform lossy compression, which is why they are not ideal for final deliveries where the video encoding would take place on the customer side after all else is said and done. For intermediary deliveries, however, movies files are excellent since they can be played out-of-the-box when reviewing. Although frames are rendered as separate images in a previous production step, there is typically some sort of "burn-in" process where additional information is added to each frame (i.e. "burnt" into the pixels). Ideally, the "burn-in" process could be combined with a video encoding step to create a movie file containing the rendered frames with the additional information added. Currently this is not possible with [Gaffer](https://github.com/GafferHQ/gaffer) since there is no way to export frames as a movie file. Similarily, it is not possible to import the frames of an existing movie file into [Gaffer](https://github.com/GafferHQ/gaffer), since it is not capable of decoding the pixel data.

What ends up happening is that the "burn in" process creates a new image sequence which must then be encoded into a video file using an external tool. This wastes disk space and creates additional files to keep track of. Now, in most cases the external tool of choice is going to be [FFmpeg](https://ffmpeg.org/), an open-source command-line tool with practically limitless possibilities when it comes to video editing. [FFmpeg](https://ffmpeg.org/) itself is an application built on top of a set of C-libraries collectively referred to as `libav`. These libraries are also open-source and are in fact part of the [FFmpeg repository](https://github.com/FFmpeg/FFmpeg). The nice part about this is that anyone can go ahead and build their own "FFmpeg application", having access to the same back-end functionality that the official [FFmpeg](https://ffmpeg.org/) has. There are several examples of similar projects targeting other host applications: 
- [Nuke](https://www.foundry.com/products/nuke-family/nuke) - [ffmpegReader](https://learn.foundry.com/nuke/developers/80/ndkreference/examples/ffmpegReader.cpp)/[ffmpegWriter](https://learn.foundry.com/nuke/developers/80/ndkreference/examples/ffmpegWriter.cpp) (seems a bit out-of-date)
- [xStudio](https://github.com/AcademySoftwareFoundation/xstudio/tree/main) - [FFmpegDecoder](https://github.com/AcademySoftwareFoundation/xstudio/blob/main/src/plugin/media_reader/ffmpeg/src/ffmpeg_decoder.hpp)
- [Chromium](https://source.chromium.org/chromium/chromium/src) - [FFmpegDecodingLoop](https://source.chromium.org/chromium/chromium/src/+/main:media/ffmpeg/ffmpeg_decoding_loop.h)

The idea of this project is to implement nodes in [Gaffer](https://github.com/GafferHQ/gaffer) that are capable of directly reading/writing movie files using `libav` as the back-end for handling decoding/encoding of frames. As mentioned, `libav` is a C-library so accessing its functionality from within a [Gaffer](https://github.com/GafferHQ/gaffer) C++ plug-in is straight-forward. The following sections describe the design of our [Gaffer](https://github.com/GafferHQ/gaffer) movie nodes and explain some of the challenges that we are currently facing.

## Design

There are many similarities between reading/writing movie files and image sequences. The biggest difference, however, is that image sequences are stored across multiple files, while movies are stored in a single file into which all the images have been encoded. Given these similarities a natural place to start are the existing [ImageReader](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/ImageReader.h)/[ImageWriter](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/ImageWriter.h) nodes in Gaffer. An important thing to note is that this node pair is hard-coded to use [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) internally to handle various image formats. Essentially, this means that a new node pair must be introduced since a different back-end is required for movie files. In the following two section we first discuss the new [Gaffer](https://github.com/GafferHQ/gaffer) nodes that are introduced. Thereafter we present some design choices made for the Gaffer-agnostic back-end required to handle movie files.

### Front-end: ilp_gaffer_movie

[Gaffer](https://github.com/GafferHQ/gaffer) integration is handled by the two nodes [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp) and [MovieWriter](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_writer.hpp). Much of the logic in these nodes is very similar to the corresponding image nodes discussed above. As for the image equivalents, most of the actual work is done be internal nodes. An example showing how this is set up can be found in [this](https://miro.com/app/board/uXjVNKEP_90=/?share_link_id=36301606464) public Miro board. Noteworthy here is that there is an internal node ([AvReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/av_reader.hpp)) that is responsible for retrieving pixel data from a movie file for a given frame. Essentially, the [AvReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/av_reader.hpp) node replaces the corresponding [OpenImageIOReader](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/OpenImageIOReader.h) in the context of retrieving pixel data. As such, our back-end for handling movie files is connected to the [AvReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/av_reader.hpp) for reading from movie files, and directly to the [MovieWriter](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_writer.hpp) for writing movie files (things are a little different when writing compared to reading since this is done by a `TaskNode`).


However, things like parsing filenumber expressions and handling various [OpenEXR](https://github.com/AcademySoftwareFoundation/openexr) properties (e.g. deep images) can be skipped, since these are not applicable to movie files. The frame masking logic still applies though, i.e. how to handle the Gaffer UI requesting a non-existing frame from a movie. 



### Back-end: ilp_movie

### Color Spaces
Handled in [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp), so we need to know what the pixel data we get from the encoder actually represents.

Links to interesting articles...


## Developer Notes

The simplest way to build the plug-ins is to use the provided Dockerfile as a [dev container](https://code.visualstudio.com/docs/devcontainers/containers). The docker image includes all the required dependencies ([Gaffer](https://github.com/GafferHQ/gaffer) and [FFmpeg](https://github.com/FFmpeg/FFmpeg)).

This project uses [CMake presets](https://devblogs.microsoft.com/cppblog/cmake-presets-integration-in-visual-studio-and-visual-studio-code/). To build the plug-ins you can use one of the provided configure presets ```gcc|clang Debug|Release```, specifying compiler and build type, respectively. If you want to use additional CMake options it is recommended to create a ```CMakeUserPresets.json``` file, where you can create a personal configure preset that inherits from one of the provided ones. That file should _not_ be part of the repository.

It is useful to set the following variables in the shell environment from which Gaffer is being launched while developing. This makes sure that Gaffer is able to load the freshly built plug-ins and that detailed log message are printed to the console.

```bash
# Change the path to wherever you install the plug-ins. 
# Note that release/debug build can exist side-by-side in different folders.
export GAFFER_EXTENSION_PATHS=$repo/out/install/unixlike-gcc-debug:$GAFFER_EXTENSION_PATHS

# Print log messages from Gaffer.
export IECORE_LOG_LEVEL=Info
```