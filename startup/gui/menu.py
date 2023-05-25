import GafferUI

import IlpGafferMovieWriter
import IlpGafferMovieWriterUI

nodeMenu = GafferUI.NodeMenu.acquire(application)
nodeMenu.append("/ILP/MovieWriterSequential", IlpGafferMovieWriter.MovieWriterSequential, searchText="ILP MovieWriterSequential")
