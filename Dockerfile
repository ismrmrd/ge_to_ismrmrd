FROM ubuntu:24.04 as ismrmrd_base

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Chicago


RUN apt-get update && apt-get install -y \
    git cmake doxygen g++ graphviz ca-certificates \
    libboost-all-dev libfftw3-dev libhdf5-serial-dev \
    zlib1g-dev libxml2-utils libpugixml-dev libxslt1-dev libtls-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/* \
    || cat /var/log/apt/term.log
    #dump into /var/log/apt/term.log in case of a failure when building the image

ENV BUILDTOP /opt/code/ge_to_ismrmrd
ENV ISMRMRD_HOME $BUILDTOP/ismrmrd
# comment the following out if you want to try the latest revision of ismrmrd
ENV ISMRMRD_VERSION d364e03d3faa3ca516da7807713b5acc72218a37  

ENV GE_TOOLS_HOME $BUILDTOP/ge-tools
ENV SDKTOP $BUILDTOP/orchestra-sdk
ENV HDF5_ROOT $SDKTOP/3p

RUN mkdir -p $BUILDTOP
COPY . $BUILDTOP

# extract Orchestra SDK
RUN cd $BUILDTOP && \
    tar xzf orchestra-sdk*.tgz && \
    rm orchestra-sdk*.tgz && \
    mv orchestra-sdk* orchestra-sdk

# ISMRMRD library
RUN cd /opt/code && \
    git clone https://github.com/ismrmrd/ismrmrd.git && \
    cd ismrmrd && \
    if [ -n "$ISMRMRD_VERSION" ]; then \
        git checkout $ISMRMRD_VERSION;\
    fi && \
    echo -n "Using ISMRMRD commit ID: " && \
    git rev-parse HEAD && \
    mkdir build && \
    cd build && \
    cmake -D CMAKE_INSTALL_PREFIX=$ISMRMRD_HOME -D build4GE=TRUE -D Boost_NO_BOOST_CMAKE=TRUE -D Boost_NO_SYSTEM_PATHS=TRUE .. && \
    make install 

# ge_to_ismrmrd conveter
RUN cd $BUILDTOP && \
    mkdir build && \
    cd build && \
    cmake -D CMAKE_INSTALL_PREFIX=$GE_TOOLS_HOME -D build4GE=TRUE -D Boost_NO_BOOST_CMAKE=TRUE -D Boost_NO_SYSTEM_PATHS=TRUE .. && \
    make -j $(nproc) && \
    make install

# Create archive of ISMRMRD libraries (including symlinks) for second stage
RUN cd $BUILDTOP/ismrmrd/lib && tar czvf libismrmrd.tgz libismrmrd*

# ----- Start another clean build without all of the build dependencies of ge_to_ismrmrd -----
FROM ubuntu:24.04

ENV GE_TOOLS_HOME /opt/code/ge_to_ismrmrd/ge-tools

RUN apt-get update && apt-get install -y --no-install-recommends libfftw3-dev libxslt1.1 libgomp1 libpugixml1v5 && apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy ge2ismrmrd from last stage and re-add necessary dependencies ($BUILDTOP does not work here)
COPY --from=ismrmrd_base /opt/code/ge_to_ismrmrd/ismrmrd/bin  /usr/local/bin
COPY --from=ismrmrd_base /opt/code/ge_to_ismrmrd/ismrmrd/lib/libismrmrd.tgz   /usr/local/lib/
COPY --from=ismrmrd_base /opt/code/ge_to_ismrmrd/ge-tools/bin/ge2ismrmrd  /usr/local/bin
COPY --from=ismrmrd_base /opt/code/ge_to_ismrmrd/ge-tools/lib/libg2i.so   /usr/local/lib/
COPY --from=ismrmrd_base /opt/code/ge_to_ismrmrd/ge-tools/ /opt/code/ge_to_ismrmrd/ge-tools/
RUN cd /usr/local/lib && tar xzf libismrmrd.tgz && rm -f libismrmrd.tgz && ldconfig
