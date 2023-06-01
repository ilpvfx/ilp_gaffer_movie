import Gaffer

import IlpGafferMovie


# fmt: off
Gaffer.Metadata.registerNode(

    IlpGafferMovie.MovieReader,

    "description",
    """
    Describe what the node does...

	Blabla ILP
    """,

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
		"out" : [
			"description",
			"""
			A pass-through of the input image.
			""",
		],
    },
)
# fmt: on
