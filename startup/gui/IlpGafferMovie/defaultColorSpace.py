import PyOpenColorIO

import IlpGafferMovie

def defaultColorSpace( fileName, fileFormat, dataType, metadata, config ) :

	linear = "" # Will be automatically substituted with the current value of `${ocio:workingSpace}`
	log = config.getColorSpace( PyOpenColorIO.ROLE_COMPOSITING_LOG ).getName()
	display = config.getColorSpace( PyOpenColorIO.ROLE_COLOR_PICKING ).getName()

	colorSpaces = {

		"bmp" : display,
		"cineon" : log,
		"dds" : display,

		"dpx" : {
			"uint8"  : display,
			"uint16" : display,
			"uint10" : log,
			"uint12" : log,
		},

		"fits" : {
			"uint8"  : display,
			"uint16" : display,
			"uint32" : linear,
			"float"  : linear,
			"double" : linear,
		},

		"ico" : display,
		"iff" : display,
		"jpeg" : display,
		"jpeg2000" : display,
		"openexr" : linear,
		"png" : display,
		"pnm" : display,
		"psd" : display,
		"raw" : linear,
		"rla" : display,
		"sgi" : display,
		"softimage" : display,
		"targa" : display,

		"tiff" : {
			"uint8"  : display,
			"uint16" : display,
			"uint32" : linear,
			"float"  : linear,
			"int8"  : linear,
			"int16"  : linear,
			"int"  : linear,
		},

		"zfile" : linear,

	}

	s = colorSpaces[fileFormat]
	if isinstance( s, str ) :
		return s
	else :
		return s[dataType]


IlpGafferMovie.MovieReader.setDefaultColorSpaceFunction( defaultColorSpace )
# IlpGafferMovie.MovieWriter.setDefaultColorSpaceFunction( defaultColorSpace )
