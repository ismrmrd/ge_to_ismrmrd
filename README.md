# Getting started with the Orchestra to ISMRMRD converter library

Orchestra conversion tools

## To build and install the tools to convert GE raw files into ISMRMRD files:

1.  Define the `SDKTOP` environment variable:

    ```bash
    export SDKTOP=/fmrif/projects/ESE/Orchestra
    ```

1. Define the `ISMRMRD_HOME` AND `GE_TOOLS_HOME` variables. These specify installation location(s), e.g.

    ```bash
    export ISMRMRD_HOME=<prefix>/ismrmrd
    export GE_TOOLS_HOME=<prefix>/ge-tools
    ```

1.  Obtain the ISMRMRD source code:

    ```bash
    git clone https://github.com/ismrmrd/ismrmrd
    ```

1.  Pre-define the location of HDF5 in order to use Orchestra's static HDF5 library:

    ```bash
    export HDF5_ROOT=$SDKTOP/3p
    ```

    Any other version of HDF5 on the system can cause conflicts as cmake will find all versions, and
    will cause issues or conflicts with the build process.  For these instructions to work, only the
    HDF5 supplied with Orchestra should be on the system.

1. Configure, compile, and install ISMRMRD:

    ```bash
    cd ismrmrd/
    git checkout GE-compat
    mkdir build
    cd build/
    cmake -D build4GE=ON -D CMAKE_INSTALL_PREFIX=$ISMRMRD_HOME -D HDF5_USE_STATIC_LIBRARIES=yes -D CMAKE_EXE_LINKER_FLAGS="-lpthread -lz -ldl" ..
    make install
    cd ../
    ```

   If the situation is encountered where the system compilers and Boost version are "too far ahead"
   of how Orchestra's support libraries (which include Boost) were compiled, then it may be necessary
   to have the ISMRMRD build explicitly refer to Orchestra's Boost libraries, with a command like:

   ```bash
   cmake -D Boost_USE_STATIC_LIBS=ON -D Boost_NO_SYSTEM_PATHS=TRUE -D BOOST_ROOT:PATHNAME=$SDKTOP/3p -D CMAKE_INSTALL_PREFIX=$ISMRMRD_HOME -D HDF5_USE_STATIC_LIBRARIES=yes -D CMAKE_EXE_LINKER_FLAGS="-lpthread -lz -ldl" ..
   ```

   A good discussion of pointing cmake to alternate Boost installations can be found at [this](
   https://stackoverflow.com/questions/3016448/how-can-i-get-cmake-to-find-my-alternative-boost-installation)
   link.

   It may also be necessary to force the usage of older ABIs standards for C++.  To accomplish this,
   a switch along the lines of:

   ```bash
   -D_GLIBCXX_USE_CXX11_ABI=0
   ```

   will have to be added to the "CMAKE_CXX_FLAGS" option in the project's CMakeLists.txt file.

1. If using the Gadgetron for reconstruction, obtain and configure code similarly, to use the HDF5 supplied with Orchestra:

    ```bash
    git clone https://github.com/gadgetron/gadgetron.git
    cd gadgetron/
    mkdir build
    cd build/
    cmake   -D CMAKE_INSTALL_PREFIX=$GADGETRON_HOME   -D HDF5_USE_STATIC_LIBRARIES=yes   -D CMAKE_EXE_LINKER_FLAGS="-lpthread -lz -ldl" ..
    make install
    cd ../
    ```

    On some systems, with multiple versions of gcc, to force Gadgetron to compile with the appropriate
    version of gcc (since at least version 6 is required), you can be explicit to cmake about the compiler
    it should use with a command like:

    ```bash
    cmake   -D CMAKE_C_COMPILER=/usr/bin/gcc-6   -D CMAKE_CXX_COMPILER=/usr/bin/g++-6   -D CMAKE_INSTALL_PREFIX=$GADGETRON_HOME   -D HDF5_USE_STATIC_LIBRARIES=yes   -D CMAKE_EXE_LINKER_FLAGS="-lpthread -lz -ldl" ..
    ```

1. Obtain the GE converter source code:

    ```bash
    git clone https://github.com/ismrmrd/ge_to_ismrmrd.git
    ```

1. Configure, compile and install the converter:

    ```bash
    cd ge_to_ismrmrd/
    mkdir build
    cd build/
    cmake -D CMAKE_INSTALL_PREFIX=$GE_TOOLS_HOME ..
    make install
    cd ../
    ```
1. Make sure `$ISMRMRD_HOME/bin` and `$GE_TOOLS_HOME/bin` are added to your environment's `PATH` variable,
   and that `$ISMRMRD_HOME/lib` and `$GE_TOOLS_HOME/lib` are added to your environment's `LD_LIBRARY_PATH`
   variable, to be able to use the libraries and binaries supplied with these tools.

1. A typical command line to convert the supplied P-file using this library is:

   ```bash
   pfile2ismrmrd -v -l libp2i-generic.so -p GenericConverter -x $GE_TOOLS_HOME/share/ge-tools/config/default.xsl P21504_FSE.7
   ```

1. If customized conversion libraries are desired, the corresponding command will be:

   ```bash
   pfile2ismrmrd -v -l libp2i-NIH.so -p NIH2dfastConverter -x $GE_TOOLS_HOME/share/ge-tools/config/default.xsl P21504_FSE.7
   ```

   The source code that enables this example is included with these tools. This example is a straightforward
   copy of the GenericConverter, but it shows how these classes can be inherited from and implemented.

1. Similarly, a typical command line to convert an example ScanArchive file using this library is:

   ```bash
   pfile2ismrmrd -v -l libp2i-generic.so -p GenericConverter -x $GE_TOOLS_HOME/share/ge-tools/config/default.xsl ScanArchive_FSE.h5
   ```

   Sample raw data files are now in the 'sampleData' directory.

