__import__("Gaffer")
__import__("GafferDispatch")

from ._IlpGafferMovie import *

__import__("IECore").loadConfig("GAFFER_STARTUP_PATHS", subdirectory="IlpGafferMovie")
