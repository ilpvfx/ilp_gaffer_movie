import IECore

import Gaffer
import GafferUI
import IlpGafferMovie

from GafferUI.PlugValueWidget import sole

from GafferImageUI import OpenColorIOTransformUI

from GafferUI.PlugValueWidget import sole

# fmt: off
Gaffer.Metadata.registerNode(

    IlpGafferMovie.MovieReader,

	"description",
	"""
	Reads video files from disk using libav (the FFmpeg backend). All file
	types supported by libav are supported by the MovieReader and all channel
	data will be converted to linear using OpenColorIO.
	""",

	plugs = {

		"fileName" : [

			"description",
			"""
			The name of the file to be read. 
			""",

			"plugValueWidget:type", "GafferUI.FileSystemPathPlugValueWidget",
			"path:leaf", True,
			#TODO(tohi): "path:bookmarks", "video", ???
			#"path:bookmarks", "image",
			"fileSystemPath:extensions", " ".join( IlpGafferMovie.MovieReader.supportedExtensions() ),
			"fileSystemPath:extensionsLabel", "Show only video files",
			"fileSystemPath:includeSequences", False,

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

		"missingFrameMode" : [

			"description",
			"""
			Determines how missing frames are handled (e.g. when the 
			requested frame is outside the movie range).
			The default behaviour is to throw an exception, but it
			can also hold the last valid frame, or return a black image 
			which matches the data window and display window of the 
			previous valid frame.
			""",

			"preset:Error", IlpGafferMovie.MovieReader.MissingFrameMode.Error,
			"preset:Black", IlpGafferMovie.MovieReader.MissingFrameMode.Black,
			"preset:Hold", IlpGafferMovie.MovieReader.MissingFrameMode.Hold,

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",

		],

		"start" : [

			"description",
			"""
			Masks frames which preceed the specified start frame.
			The default is to treat them based on the MissingFrameMode,
			but they can be clamped to the start frame, or return a 
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
			The default is to treat them based on the MissingFrameMode,
			but they can be clamped to the end frame, or return a 
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

		"colorSpace" : [

			"description",
			"""
			The colour space of the input frame, used to convert the input to
			the working space. When set to `Automatic`, the colour space is
			determined automatically using the function registered with
			`MovieReader::setDefaultColorSpaceFunction()`.
			""",

			"presetNames", OpenColorIOTransformUI.colorSpacePresetNames,
			"presetValues", OpenColorIOTransformUI.colorSpacePresetValues,
			"openColorIO:categories", "file-io",
			"openColorIO:extraPresetNames", IECore.StringVectorData( [ "Automatic" ] ),
			"openColorIO:extraPresetValues", IECore.StringVectorData( [ "" ] ),

			"plugValueWidget:type", "IlpGafferMovieUI.MovieReaderUI._ColorSpacePlugValueWidget",

		],

		"videoStream" : [

			"description",
			"""
			The video stream to be read. By default try to read the 'best' stream, as suggested 
			by the decoder. To read a specific video stream provide a value on the form 'N',
			where N is an integer corresponding to the stream index, e.g. '1'.
			""",

			"label", "Video Stream",

		],

		"filterGraph" : [

			"description",
			"""
			The filter graph applied to the decoded frames.\n
			FFmpeg command-line equivalent of '-vf'.
			""",

			"label", "Filter Graph",
			"plugValueWidget:type", "GafferUI.MultiLineStringPlugValueWidget",

		],

		# section: Frames

		"availableFrames" : [

			"description",
			"""
			A list of the available frames for the current file.
			""",

			"layout:section", "Frames",
			"plugValueWidget:type", "IlpGafferMovieUI.MovieReaderUI._AvailableFramesPlugValueWidget",

		],

		"fileValid" : [

			"description",
			"""
			Whether or not the file exists and can be read into memory. Additionally, the 
			file must contain at least one video stream.
			""",

			"layout:section", "Frames",

		],

		"probe" : [
			"description",
			"""
			Information about the video file, the output is similar to that from
			ffprobe.
			""",

			"layout:section", "Frames",
			"plugValueWidget:type", "GafferUI.MultiLineStringPlugValueWidget",
			"label", "Probe",

		]

	}

)

# Augments PresetsPlugValueWidget label with the computed colour space
# when preset is "Automatic". Since this involves opening the file to
# read metadata, we do the work in the background via an auxiliary plug
# passed to `_valuesForUpdate()`.
class _ColorSpacePlugValueWidget( GafferUI.PresetsPlugValueWidget ) :

	def __init__( self, plugs, **kw ) :

		GafferUI.PresetsPlugValueWidget.__init__( self, plugs, **kw )

	@staticmethod
	def _valuesForUpdate( plugs, auxiliaryPlugs ) :

		presets = GafferUI.PresetsPlugValueWidget._valuesForUpdate( plugs, [ [] for p in plugs ] )

		result = []
		for preset, colorSpacePlugs in zip( presets, auxiliaryPlugs ) :

			automaticSpace = ""
			if len( colorSpacePlugs ) and preset == "Automatic" :
				with IECore.IgnoredExceptions( Gaffer.ProcessException ) :
					automaticSpace = colorSpacePlugs[0].getValue() or "Working Space"

			result.append( {
				"preset" : preset,
				"automaticSpace" : automaticSpace
			} )

		return result

	def _updateFromValues( self, values, exception ) :

		GafferUI.PresetsPlugValueWidget._updateFromValues( self, [ v["preset"] for v in values ], exception )

		if self.menuButton().getText() == "Automatic" :
			automaticSpace = sole( v["automaticSpace"] for v in values )
			if automaticSpace != "" :
				self.menuButton().setText( "Automatic ({})".format( automaticSpace or "---" ) )

	def _auxiliaryPlugs( self, plug ) :

		node = plug.node()
		if isinstance( node, IlpGafferMovie.MovieReader ) :
			return [ node["__intermediateColorSpace"] ]


class _AvailableFramesPlugValueWidget( GafferUI.PlugValueWidget ) :

	def __init__( self, plug, **kw ) :

		self.__textWidget = GafferUI.TextWidget( editable = False )
		GafferUI.PlugValueWidget.__init__( self, self.__textWidget, plug, **kw )

	def _updateFromValues( self, values, exception ) :

		value = sole( values )
		if value is None :
			self.__textWidget.setText( "---" )
		else :
			self.__textWidget.setText( str( IECore.frameListFromList( list( value ) ) ) )

		self.__textWidget.setErrored( exception is not None )

# fmt: on
