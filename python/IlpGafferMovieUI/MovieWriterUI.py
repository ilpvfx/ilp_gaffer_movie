import os

import Gaffer
import GafferUI
import IlpGafferMovie


def __extension( parent ) :

	fileNamePlug = parent["fileName"]
	fileName = fileNamePlug.getValue()
	ext = os.path.splitext( fileName )[1]
	if ext :
		return ext.lower()[1:]
	return "" if fileNamePlug.isSetToDefault() else "unknown"


# fmt: off
Gaffer.Metadata.registerNode(

    IlpGafferMovie.MovieWriter,

    "description",
    """
    Describe what the node does...

	Blabla ILP
    """,

	"layout:activator:mov", lambda p : __extension( p ) in ( "mov", "unknown" ),
	"layout:activator:h264", lambda p : p["codec"].getValue() in ( "h264" ),
	# "layout:activator:h264_advanced", lambda p : p["codec"].getValue() in ( "h264" ) and p["h264"]["preset"].getValue() in ( "custom" ),
	"layout:activator:prores", lambda p : p["codec"].getValue() in ( "prores" ),

    plugs = {
		"in" : [
			"description",
			"""
			The image to be written to disk.
			""",

			"nodule:type", "GafferUI::StandardNodule",
		],        
		"fileName" : [
			"description",
			"""
			The name of the file to be written.
			The file format is guessed automatically from the extension, e.g. 
			/home/my_user/my_video.mov will create a file using the mov-format.
			""",

			"plugValueWidget:type", "GafferUI.FileSystemPathPlugValueWidget",
			"path:leaf", True,
		],
		# "format" : [
		# 	"description",
		# 	"""
		# 	Fancy description! TBD
		# 	""",

		# 	"preset:Mov", "mov",
		# 	"preset:Mp4", "mp4",

		# 	"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
		# ],
		# "frameRate" : [
		# 	"description",
		# 	"""
		# 	Fancy description! TBD
		# 	""",
		# ],
		"colorRange" : [
			"description",
			"""
			FFmpeg command-line equivalent of '-color_range':
			- pc: Full Range (JPEG)
			- tv: Video Range (MPEG)
			""",

			"label", "Color Range",

			"preset:pc", "pc",
			"preset:tv", "tv",
			"preset:unknown", "unknown",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
		],
		"colorspace" : [
			"description",
			"""
			FFmpeg command-line equivalent of '-colorspace'.
			""",

			"label", "Colorspace",

			"preset:bt709", "bt709",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
		],
		"colorPrimaries" : [
			"description",
			"""
			FFmpeg command-line equivalent of '-color_primaries'.
			""",

			"label", "Color Primaries",

			"preset:bt709", "bt709",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
		],
		"colorTrc" : [
			"description",
			"""
			FFmpeg command-line equivalent of '-color_trc'.
			""",

			"label", "Color Transfer",

			"preset:bt709", "bt709",
			"preset:iec61966-2-1", "iec61966-2-1",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
		],
		"swsFlags" : [
			"description",
			"""
			FFmpeg command-line equivalent of '-sws_flags'.
			""",

			"label", "SWS Flags",
		],
		"filtergraph" : [
			"description",
			"""
			FFmpeg command-line equivalent of '-vf'.
			""",

			"label", "Filtergraph",
		],
		"codec" : [
			"description",
			"""
			The codec to use for encoding video.
			""",

			"preset:ProRes", "prores",
			"preset:H264", "h264",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
		],
		"h264" : [
			"description",
			"""
			Options specific to H.264 codecs.
			""",

			"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",
			"layout:section", "Settings.H264",
			"layout:visibilityActivator", "h264",
		],
		"h264.profile" : [
			"description",
			"""
			A fast encoder produces large files and vice versa.
			The available presets in descending order of speed are:
			- "baseline" 
			- "main" 
			- "high" 
			- "high10" (first 10 bit compatible profile)
			- "high422" (supports yuv420p, yuv422p, yuv420p10le and yuv422p10le)
			- "high444" (supports as above as well as yuv444p and yuv444p10le)
			""",

			"label", "Profile",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"preset:Baseline",          "baseline",
			"preset:Main",              "main",
			"preset:High 4:2:0 8-bit",  "high",
			"preset:High 4:2:0 10-bit", "high10",
			"preset:High 4:2:2 8-bit",  "high422_8b",
			"preset:High 4:2:2 10-bit", "high422_10b",
			"preset:High 4:4:4 8-bit",  "high444_8b",
			"preset:High 4:4:4 10-bit", "high444_10b",
		],
		"h264.preset" : [
			"description",
			"""
			A preset is a collection of options that will provide a certain encoding speed to 
			compression ratio. A slower preset will provide better compression (compression is 
			quality per filesize). This means that, for example, if you target a certain file 
			size or constant bit rate, you will achieve better quality with a slower preset. 
			Similarly, for constant quality encoding, you will simply save bitrate by choosing 
			a slower preset.			

			A fast encoder produces large files and vice versa.
			Use the slowest preset that you have patience for. 
			The available presets in descending order of speed are:
			- "ultrafast" 
			- "superfast" 
			- "veryfast" 
			- "faster" 
			- "fast" 
			- "medium" 
			- "slow" 
			- "slower" 
			- "veryslow" 
			""",

			"label", "Preset",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"preset:Ultra fast", "ultrafast",
			"preset:Super fast", "superfast",
			"preset:Very fast", "veryfast",
			"preset:Faster", "faster",
			"preset:Fast", "fast",
			"preset:Medium", "medium",
			"preset:Slow", "slow",
			"preset:Slower", "slower",
			"preset:Very slow", "veryslow",
			"preset:Custom", "custom",
		],
		"h264.tune" : [
			"description",
			"""
    		Tuning options:
    		- "Film" - use for high quality movie content; lowers deblocking.
    		- "Animation" - good for cartoons; uses higher deblocking and more reference frames.
    		- "Grain" - preserves the grain structure in old, grainy film material.
    		- "Still image" - good for slideshow-like content.
    		- "Fast decode" - allows faster decoding by disabling certain filters.
    		- "Zero latency" - good for fast encoding and low-latency streaming.
			""",

			"label", "Tune",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"preset:None", "none",
			"preset:Film", "film",
			"preset:Animation", "animation",
			"preset:Grain", "grain",
			"preset:Still image", "stillimage",
			"preset:Fast decode", "fastdecode",
			"preset:Zero latency", "zerolatency",
		],
		"h264.crf" : [
			"description",
			"""
			Constant rate factor. 
			Should be in range [0, 51], where lower values result in higher quality but 
			larger files, and vice versa. Sane values are in the range [18, 28].
			A change of +/-6 should result in about half/double the file size, but this is
			just an estimate.
			""",

			"label", "CRF",
		],
		"h264.x264Params" : [
			"description",
			"""
			Parameters specific to libx264. 
			Given as "key0=value0:key1=value1".
			""",

			"label", "x264 Params",
		],
		"h264.qp" : [
			"description",
			"""
			Constant quantization parameter rate control. 
			Set to -1 to disable (use defaults).
			""",

			"label", "QP",
		],

		# "h264Advanced" : [
		# 	"description",
		# 	"""
		# 	Advanced options.
		# 	""",

		# 	"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",
		# 	"layout:section", "Settings.Advanced",
		# 	# "layout:section:h264Advanced:collapsed", False,
		# 	"layout:visibilityActivator", "h264_advanced",
		# ],		
		# "h264Advanced.gopSize" : [
		# 	"description",
		# 	"""
		# 	Sets how many frames can be placed together to form a compression GOP (group of pictures).
		# 	\n
		# 	Use caution with this control as large alterations can stop other applications reading the rendered file.
		# 	""",

		# 	"label", "GOP Size",
		# ],
		# "h264Advanced.bFrames" : [
		# 	"description",
		# 	"""
		# 	Sets the maximum number of B frames that can be consecutive in the rendered file.
		# 	\n
		# 	The default, 0, does not impose any maximum number of B frames in the output.
		# 	""",

		# 	"label", "B Frames",
		# ],
		# "h264Advanced.bitrate" : [
		# 	"description",
		# 	"""
		# 	Sets the target bitrate that the codec attempts to reach, within the limits set by 
		# 	the Bitrate Tolerance and Quantizer Min/Max controls.
		# 	""",

		# 	"label", "Bitrate",
		# ],
		# "h264Advanced.bitrateTolerance" : [
		# 	"description",
		# 	"""
		# 	Sets the amount that the bitrate can vary from the Bitrate setting. 
		# 	Setting this tolerance too low can result in renders failing.
		# 	""",

		# 	"label", "Bitrate Tolerance",
		# ],
		# "h264Advanced.qmin" : [
		# 	"description",
		# 	"""
		# 	Sets the quality range within which the codec can vary the image to achieve the specified 
		# 	bitrate. Higher ranges can introduce image degradation.			
		# 	""",

		# 	"label", "Quantizer Min",
		# ],
		# "h264Advanced.qmax" : [
		# 	"description",
		# 	"""
		# 	Sets the quality range within which the codec can vary the image to achieve the specified 
		# 	bitrate. Higher ranges can introduce image degradation.			
		# 	""",

		# 	"label", "Quantizer Max",
		# ],
		"prores" : [
			"description",
			"""
			Options specific to ProRes codecs.
			""",

			"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",
			"layout:section", "Settings.ProRes",
			"layout:visibilityActivator", "prores",
		],		
		"prores.profile" : [
			"description",
			"""
			- "proxy" 
			- "lt" 
			- "standard"
			- "hq"
			TODO!
			""",

			"label", "Profile",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"preset:Apple ProRes Proxy 4:2:2 10-bit",  "proxy_10b",
			"preset:Apple ProRes LT 4:2:2 10-bit",  "lt_10b",
			"preset:Apple ProRes 4:2:2 10-bit",  "standard_10b",
			"preset:Apple ProRes HQ 4:2:2 10-bit",  "hq_10b",
			"preset:Apple ProRes 4444 4:4:4 10-bit",  "4444_10b",
			"preset:Apple ProRes 4444 XQ 4:4:4 10-bit",  "4444xq_10b",
		],
		"prores.qscale" : [
			"description",
			"""
			TODO!
			""",

			"label", "Q-scale",
		],

		"out" : [
			"description",
			"""
			A pass-through of the input image.
			""",
		],
    },
)
# fmt: on
