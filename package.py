name = "ilp_gaffer_movie"

@early()
def version():
    return open("VERSION.txt").read().strip()

private_build_requires = [
    "cmake-3.21+"
]

requires = [
    "gaffer-1",
    "ffmpeg-5.1"
]

build_command = """
cmake {root} \
    -DGAFFER_ROOT=$REZ_GAFFER_ROOT \
    -DFFMPEG_ROOT=$REZ_FFMPEG_ROOT \
    -DPYTHON_VERSION=$REZ_PYTHON_MAJOR_VERSION.$REZ_PYTHON_MINOR_VERSION \
    -DCMAKE_INSTALL_PREFIX=$REZ_BUILD_INSTALL_PATH \
    -DCMAKE_MODULE_PATH=$CMAKE_MODULE_PATH \
    -DCMAKE_BUILD_TYPE=Release
if [[ $REZ_BUILD_INSTALL -eq 1 ]];
then
    make install
fi
"""

def commands():
    env.GAFFER_EXTENSION_PATHS.append("{root}")
