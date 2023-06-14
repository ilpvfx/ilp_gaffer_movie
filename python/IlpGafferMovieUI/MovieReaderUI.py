import IECore

import Gaffer
import GafferUI
import IlpGafferMovie


# fmt: off
Gaffer.Metadata.registerNode(

    IlpGafferMovie.MovieReader,

	"description",
	"""
	Reads video files from disk using libav (the FFmpeg backend). All file
	types supported by libav are supported by the MovieReader.
	""",

	plugs = {

		"fileName" : [

			"description",
			"""
			The name of the file to be read. 
			""",

			"plugValueWidget:type", "GafferUI.FileSystemPathPlugValueWidget",
			"path:leaf", True,
			#"path:bookmarks", "image",
			# TODO(tohi): get supported extensions from libav?
			# "fileSystemPath:extensions", " ".join( GafferImage.ImageReader.supportedExtensions() ),
			#"fileSystemPath:extensionsLabel", "Show only video files",
			#"fileSystemPath:includeSequences", True,

		],

		"refreshCount" : [

			"description",
			"""
			May be incremented to force a reload if the file has
			changed on disk - otherwise old contents may still
			be loaded via Gaffer's cache.
			""",

			"plugValueWidget:type", "GafferUI.RefreshPlugValueWidget",
			"layout:label", "",
			"layout:accessory", True,

		],

		"start" : [

			"description",
			"""
			Masks frames which preceed the specified start frame.
			They can be clamped to the start frame, or return a 
			black image which matches the data window and display 
			window of the start frame.
			""",

			"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",
			"layoutPlugValueWidget:orientation", "horizontal",

			"layout:activator:modeIsNotNone", lambda plug : plug["mode"].getValue() != IlpGafferMovie.MovieReader.FrameMaskMode.None_,

		],

		"start.mode" : [

			"description",
			"""
			The mode used to determine the mask behaviour for the start frame.
			""",

			"preset:None", IlpGafferMovie.MovieReader.FrameMaskMode.None_,
			"preset:Black Outside", IlpGafferMovie.MovieReader.FrameMaskMode.BlackOutside,
			"preset:Clamp to Range", IlpGafferMovie.MovieReader.FrameMaskMode.ClampToFrame,

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"layout:label", "",

		],

		"start.frame" : [

			"description",
			"""
			The start frame of the masked range.
			""",

			# TODO(tohi): One entry per frame? Seems a bit strange...
			#"presetNames", lambda plug : IECore.StringVectorData( [ str(x) for x in plug.node()["__oiioReader"]["availableFrames"].getValue() ] ),
			#"presetValues", lambda plug : plug.node()["__oiioReader"]["availableFrames"].getValue(),

			"layout:label", "",
			"layout:activator", "modeIsNotNone",

		],

		"end" : [

			"description",
			"""
			Masks frames which follow the specified end frame.
			They can be clamped to the end frame, or eturn a 
			black image which matches the data window and display 
			window of the end frame.
			""",

			"plugValueWidget:type", "GafferUI.LayoutPlugValueWidget",
			"layoutPlugValueWidget:orientation", "horizontal",

			"layout:activator:modeIsNotNone", lambda plug : plug["mode"].getValue() != IlpGafferMovie.MovieReader.FrameMaskMode.None_,

		],

		"end.mode" : [

			"description",
			"""
			The mode used to determine the mask behaviour for the end frame.
			""",

			"preset:None", IlpGafferMovie.MovieReader.FrameMaskMode.None_,
			"preset:Black Outside", IlpGafferMovie.MovieReader.FrameMaskMode.BlackOutside,
			"preset:Clamp to Range", IlpGafferMovie.MovieReader.FrameMaskMode.ClampToFrame,

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"layout:label", "",

		],

		"end.frame" : [

			"description",
			"""
			The end frame of the masked range.
			""",

			# TODO(tohi): One entry per frame? Seems a bit strange...
			# "presetNames", lambda plug : IECore.StringVectorData( [ str(x) for x in plug.node()["__oiioReader"]["availableFrames"].getValue() ] ),
			# "presetValues", lambda plug : plug.node()["__oiioReader"]["availableFrames"].getValue(),

			"layout:label", "",
			"layout:activator", "modeIsNotNone",

		],

	}
)
# fmt: on
