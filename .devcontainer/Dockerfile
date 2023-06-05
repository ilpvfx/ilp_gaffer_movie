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

#RUN cd ~ && git clone --depth 1 --branch 2.112.0 https://github.com/AcademySoftwareFoundation/rez.git
#RUN cd ~/rez && python ./install.py -v ~/rez-local
#RUN export PATH=/home/vscode/rez-local/bin/rez:$PATH
WORKDIR /opt
RUN git clone https://github.com/piratecrew/rez-ffmpeg.git && \
    mkdir rez-ffmpeg/build && cd rez-ffmpeg/build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/opt/ffmpeg && make -j11

#RUN cd ~/rez-ffmpeg.git && rez-build --install