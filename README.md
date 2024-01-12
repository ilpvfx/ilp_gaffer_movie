# Gaffer Movie Nodes
At a high level, [Gaffer](https://github.com/GafferHQ/gaffer) can already read/write movies, since movies are essentially a sequence of images played back at a high enough frame rate to trick the human eye into interpreting the result as a continuous visual stream. To date, this is done by reading/writing image sequences in one of many popular formats (`.exr`, `.png`, or whatever [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) supports). As such movie frames are assumed to exist on disk as separate image files, and compression is thus only possible at a per-frame level. Now, while an image sequence might be suitable for a final delivery to a client, it is not always ideal to work with since data is spread across many (large-ish) files. Additionally, most (if not all) media players require image sequences to be encoded into a single movie file using one of many efficient video encoders (e.g. `ProRes`, `H264`, etc). Video encoders drastically reduce the size of an image sequence by performing temporal compression and many other tricks. The resulting pixel data is then stored in a bitstream that can be efficiently read from disk, which enables playback at the intended frame rate. 

Video encoders typically perform lossy compression, which is why they are not ideal for final deliveries where the video encoding would take place on the customer side after all else is said and done. For intermediary deliveries, however, movies files are excellent since they can be played out-of-the-box when reviewing. Although frames are rendered as separate images in a previous production step, there is typically some sort of "burn-in" process where additional information is added to each frame (i.e. "burnt" into the pixels). Ideally, the "burn-in" process could be combined with a video encoding step to create a movie file containing the rendered frames with the additional information added. Currently this is not possible with [Gaffer](https://github.com/GafferHQ/gaffer) since there is no way to export frames as a movie file. Similarily, it is not possible to import the frames of an existing movie file into [Gaffer](https://github.com/GafferHQ/gaffer), since it is not capable of decoding the pixel data.

What ends up happening is that the "burn in" process creates a new image sequence which must then be encoded into a video file using an external tool. This wastes disk space and creates additional files to keep track of. Now, in most cases the external tool of choice is going to be [FFmpeg](https://ffmpeg.org/), an open-source command-line tool with practically limitless possibilities when it comes to video editing. [FFmpeg](https://ffmpeg.org/) itself is an application built on top of a set of C-libraries collectively referred to as `libav`. These libraries are also open-source and are in fact part of the [FFmpeg repository](https://github.com/FFmpeg/FFmpeg). The nice part about this is that anyone can go ahead and build their own "FFmpeg application", having access to the same back-end functionality that the official [FFmpeg](https://ffmpeg.org/) has. There are several examples of similar projects targeting other host applications: 
- [Nuke](https://www.foundry.com/products/nuke-family/nuke) - [ffmpegReader](https://learn.foundry.com/nuke/developers/80/ndkreference/examples/ffmpegReader.cpp)/[ffmpegWriter](https://learn.foundry.com/nuke/developers/80/ndkreference/examples/ffmpegWriter.cpp) (seems a bit out-of-date)
- [xStudio](https://github.com/AcademySoftwareFoundation/xstudio/tree/main) - [FFmpegDecoder](https://github.com/AcademySoftwareFoundation/xstudio/blob/main/src/plugin/media_reader/ffmpeg/src/ffmpeg_decoder.hpp)
- [Chromium](https://source.chromium.org/chromium/chromium/src) - [FFmpegDecodingLoop](https://source.chromium.org/chromium/chromium/src/+/main:media/ffmpeg/ffmpeg_decoding_loop.h)

The idea of this project is to implement nodes in [Gaffer](https://github.com/GafferHQ/gaffer) that are capable of directly reading/writing movie files using `libav` as the back-end for handling decoding/encoding of frames. As mentioned, `libav` is a C-library so accessing its functionality from within a [Gaffer](https://github.com/GafferHQ/gaffer) C++ plug-in is straight-forward. The following sections describe the design of our [Gaffer](https://github.com/GafferHQ/gaffer) movie nodes and explain some of the challenges that we are currently facing.

## Design

There are many similarities between reading/writing movie files and image sequences. The biggest difference, however, is that image sequences are stored across multiple files, while movies are stored in a single file into which all the images have been encoded. Given these similarities a natural place to start are the existing [ImageReader](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/ImageReader.h)/[ImageWriter](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/ImageWriter.h) nodes in [Gaffer](https://github.com/GafferHQ/gaffer). An important thing to note is that this node pair is hard-coded to use [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) internally to handle various image formats. Essentially, this means that a new node pair must be introduced since a different back-end is required for movie files. 

In the following two section we first discuss the new [Gaffer](https://github.com/GafferHQ/gaffer) nodes that are introduced. Thereafter we present some design choices made for the Gaffer-agnostic back-end required to handle movie files.

### Front-end: ilp_gaffer_movie

[Gaffer](https://github.com/GafferHQ/gaffer) integration is handled mainly by introducing the two nodes [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp) and [MovieWriter](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_writer.hpp). Much of the logic in these nodes is very similar to the corresponding image nodes discussed above. Also, the UI components are intentionally similar to the image counterparts since the workflows inside [Gaffer](https://github.com/GafferHQ/gaffer) are likely similar regardless of pixel data coming from an image sequence or a movie file. 

As for Gaffer's [ImageReader](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/ImageReader.h), most of the actual work in the [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp) is done by internal nodes. An illustration showing the internal setup can be found in [this](https://miro.com/app/board/uXjVNKEP_90=/?share_link_id=36301606464) public Miro board. Noteworthy here is that there is an internal node ([AvReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/av_reader.hpp)) responsible for retrieving pixel data from a movie file for a given frame. Essentially, the [AvReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/av_reader.hpp) node replaces the corresponding [OpenImageIOReader](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/OpenImageIOReader.h) in the context of retrieving pixel data from disk. One thing to note is that color space management is handled internally by the [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp) node. As it turns out, reconciling the color space management between [OpenColorIO](https://github.com/AcademySoftwareFoundation/OpenColorIO) and `libav` is not trivial, which is discussed later on.

The [AvReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/av_reader.hpp) node uses our back-end, discussed in the following section, to retrieve pixel data from movie files. It also uses a frame cache to support more interactive "scrubbing" by storing some amount of frames that have previously been decoded. Although similar to its [OpenImageIOReader](https://github.com/GafferHQ/gaffer/blob/main/include/GafferImage/OpenImageIOReader.h) counterpart, things like parsing file number expressions and handling various [OpenEXR](https://github.com/AcademySoftwareFoundation/openexr) properties (e.g. deep images) can be skipped, since these are not applicable to movie files. Frame masking logic, however, still applies, including how to handle requests for non-existing frames of a movie. 

The [MovieWriter](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_writer.hpp) is arguably simpler than the [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp) and is implemented as a `TaskNode`. Here the goal is to export frames from within [Gaffer](https://github.com/GafferHQ/gaffer) to a movie file on disk. The heavy lifting in terms of encoding is done by our back-end, as will be discussed next. It is noteworthy that color space needs to be handled in the case of writing as well, but in general writing is simpler than reading since it is mostly about generating pixel data for a given, fixed frame range.

### Back-end: ilp_movie

As mentioned, [FFmpeg](https://ffmpeg.org/) is a command-line application used for video editing. In particular, it is probably fair to say, [FFmpeg](https://ffmpeg.org/) is used for various encoding tasks in which image sequences are transformed into movie files (or movie files being "transcoded" into modified movie files). Now, in order to perform similar tasks within an application it is not possible to use [FFmpeg](https://ffmpeg.org/) directly without first launching it as a sub-process and somehow piping the pixel data to that process. An example of how this might be done using a Python node in [Gaffer](https://github.com/GafferHQ/gaffer) is given in the [appendix](#ffmpeg-encode-in-python).

The main benefit of doing it this way is that there are plenty of recommendations available on how to use [FFmpeg](https://ffmpeg.org/) to achieve satisfactory results. These recommendations can be followed by simply using the suggested command-line options. For instance, the [ASWF Open Review Initiative](https://academysoftwarefoundation.github.io/EncodingGuidelines/) (ORI) has published encoding guidelines for the VFX community targeting a number of different codecs. As an example, here are the recommended parameters when using a `H264` encoder to make a movie suitable for web review:
```
ffmpeg -r 24 -start_number 100 -i inputfile.%04d.png -frames:v 200 -c:v libx264 \
        -pix_fmt yuv420p10le -crf 18 -preset slow \
        -sws_flags spline+accurate_rnd+full_chroma_int \
        -vf "scale=in_range=full:in_color_matrix=bt709:out_range=tv:out_color_matrix=bt709" \
        -color_range 1 -colorspace 1 -color_primaries 1 -color_trc 2 \
        -movflags faststart -y outputfile.mp4
```
An explanation of these parameters can be found [here](https://academysoftwarefoundation.github.io/EncodingGuidelines/Encodeh264.html). Similar recommendations are available for other codecs as well and they follow the pattern of converting an image sequence on disk (`inputfile.####.*`) to a movie file (`outputfile.*`). 

As mentioned above, the [FFmpeg](https://ffmpeg.org/) command-line application is implemented on top of a set of libraries known as `libav`. Essentially, this enables developers to implement FFmpeg-like behaviour in other applications using the `libav` libraries. However, in order to be compliant with recommended [FFmpeg](https://ffmpeg.org/) commands, such implementations support the relevant parameters and must interpret every parameter _exactly_ the same way that [FFmpeg](https://ffmpeg.org/) does. Now, while [FFmpeg](https://ffmpeg.org/) is open-source, making it possible to see what's going on, it supports a staggering amount of different parameters. Granted, all of these parameters are not relevant, but digging through the source code to find specific details can be a daunting task.

To encapsulate some of the complexities that come with using `libav`, a back-end module called `ilp_movie` was introduced. The main functionality of this module is take an image that resides in memory (as opposed to being stored in a file on disk) and pass that image to an encoder (and vice versa for decoding). This enables skipping the step of converting one image sequence into another on disk before encoding, and allows pixel data from movie files to be presented in [Gaffer](https://github.com/GafferHQ/gaffer). An advantage to this approach is that `ilp_movie` could potentially be re-used in other projects, since it has no dependecies on [Gaffer](https://github.com/GafferHQ/gaffer).

Furthermore, `ilp_movie` uses `libav` as a _private_ dependency (as opposed to _public_/_transitive_). This means that users of `ilp_movie` are not even aware that `libav` is used in the implementation. While this requires some additional work, it means that `ilp_movie` can link statically against `libav` and that `ilp_movie` can be distributed without providing any `libav` header files. The main challenge, however, is to make sure that `ilp_movie` behaves _exactly_ like [FFmpeg](https://ffmpeg.org/) when it comes to interpreting the parameters passed to it.


### Color Space
Handled in [MovieReader](https://github.com/ilpvfx/ilp_gaffer_movie/blob/main/include/ilp_gaffer_movie/movie_reader.hpp), so we need to know what the pixel data we get from the encoder actually represents.

Links to interesting articles...


[FFmpeg color space](https://trac.ffmpeg.org/wiki/colorspace)


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


## Appendix

### FFmpeg Encode in Python

A simplified example of how a [Gaffer](https://github.com/GafferHQ/gaffer) node authored in Python can be used to pipe data to [FFmpeg](https://ffmpeg.org/) running as a sub-process.

```python
import shlex
import subprocess
from typing import List

import IECore

import Gaffer
import GafferDispatch
import GafferImage


class FFmpegEncodeVideo(GafferDispatch.TaskNode):
    def __init__(self, name: str = "FFmpegEncodeVideo") -> None:
        super().__init__(name)

        self["in"] = GafferImage.ImagePlug()
        self["out"] = GafferImage.ImagePlug(direction=Gaffer.Plug.Direction.Out)
        self["out"].setInput(self["in"])

        self["fileName"] = Gaffer.StringPlug()
        self["codec"] = Gaffer.StringPlug(
            defaultValue="-c:v mjpeg -qscale:v 2 -vendor ap10 -pix_fmt yuvj444p"
        )

    def executeSequence(self, frames: List[float]) -> None:
        with Gaffer.Context(Gaffer.Context.current()) as context:
            context.setFrame(frames[0])

            if self["in"].deep():
                raise RuntimeError("Deep is not supported...")

            cmd = "ffmpeg -y -f rawvideo -pix_fmt gbrpf32le -s {format} -r {framesPerSecond} -i - {codec} {fileName}".format(
                format=self["in"]["format"].getValue(),
                framesPerSecond=context.getFramesPerSecond(),
                codec=self["codec"].getValue(),
                fileName=self["fileName"].getValue()
            )
            proc = subprocess.Popen(shlex.split(cmd), stdin=subprocess.PIPE, stderr=subprocess.PIPE)
            for frame in frames:
                context.setFrame(frame)

                img = GafferImage.ImageAlgo.image(self["in"])
                proc.stdin.write(img["G"].toString())
                proc.stdin.write(img["B"].toString())
				proc.stdin.write(img["R"].toString())

                IECore.msg(
                    IECore.Msg.Level.Info,
                    self.relativeName(self.scriptNode()),
                    f"Writing {frame}",
                )

            proc.stdin.close()
            proc.wait()

    def hash(self, context: Gaffer.Context) -> IECore.MurmurHash:
        h = super().hash(context)
        h.append(context.getFrame())

        return h

    def requiresSequenceExecution(self) -> bool:
        return True


Gaffer.Metadata.registerNode(

    FFmpegEncodeVideo,

    plugs={
        "in": [
            "nodule:type", "GafferUI::StandardNodule",
        ],
        "fileName": [
            "plugValueWidget:type", "GafferUI.FileSystemPathPlugValueWidget",
            "fileSystemPath:extensions", "mov",
            "fileSystemPath:extensionsLabel", "Show only supported files",
            "fileSystemPath:includeSequences", False,
        ]
    },
)


IECore.registerRunTimeTyped(
    FFmpegEncodeVideo, typeName="Foobar::FFmpegEncodeVideo"
)
```
