FROM aswf/ci-base:2022

# RUN yum install -y gcc-toolset-9-libasan-devel

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

# [Optional] Set the default user. Omit if you want to keep the default as root.
# USER $USERNAME

# WORKDIR /opt
RUN git clone --depth 1 --branch ffmpeg-5.1 https://github.com/piratecrew/rez-ffmpeg.git /tmp/rez-ffmpeg && \
    mkdir /tmp/rez-ffmpeg/build && cd /tmp/rez-ffmpeg/build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/opt/ffmpeg-5.1 && make -j11

RUN cd /tmp && wget https://github.com/GafferHQ/gaffer/releases/download/1.2.0.2/gaffer-1.2.0.2-linux.tar.gz && \
    mkdir -p /opt/gaffer-1.2.0.2 && tar -xf gaffer-1.2.0.2-linux.tar.gz -C /opt/gaffer-1.2.0.2 --strip-components=1
