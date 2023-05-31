import GafferUI

import IlpGafferMovie
import IlpGafferMovieUI

nodeMenu = GafferUI.NodeMenu.acquire(application)
nodeMenu.append("/ILP/Movie/MovieReader", IlpGafferMovie.MovieReader, searchText="ILP MovieReader")
nodeMenu.append("/ILP/Movie/MovieWriter", IlpGafferMovie.MovieWriter, searchText="ILP MovieWriter")
