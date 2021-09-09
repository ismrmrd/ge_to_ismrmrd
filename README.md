# Getting started with the Orchestra to ISMRMRD converter library

Orchestra conversion tools

## To build and install the tools to convert GE raw files into ISMRMRD files:

1.  To use Orchestra's version 2 SDK to build ISMRMRD and GE-to-ISMRMRD, when trying to compile with
    the shipping configuration, an error message of:

   ```bash
      tlslayer.cc:(.text+0x1bed): undefined reference to `RAND_egd'
   ```

   is generated on openSUSE 15.2.  This happens because in current versions of OpenSSL, building the
   "Entropy Gathering Daemon" is disabled by default on openSUSE.  However, since this function call
   is in a library shipping with Orchestra 2.0, then on openSUSE, OpenSSL must be built and installed
   into Orchestra's environment, with the appropriate option enabled.

   To do this, first obtain the flags with which OpenSSL is built, by using the command:

   ```bash
      openssl version -f
   ```

   On openSUSE 15.2, this output of this command is:

   ```bash
      compiler: gcc -fPIC -pthread -m64 -Wa,--noexecstack -Wall -O3 -fmessage-length=0 -grecord-gcc-switches -O2 -Wall -fstack-protector-strong -funwind-tables -fasynchronous-unwind-tables -fstack-clash-protection -g -Wa,--noexecstack -fno-common -Wall -DOPENSSL_USE_NODELETE -DL_ENDIAN -DOPENSSL_PIC -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_BN_ASM_GF2m -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DKECCAK1600_ASM -DRC4_ASM -DMD5_ASM -DVPAES_ASM -DGHASH_ASM -DECP_NISTZ256_ASM -DX25519_ASM -DPOLY1305_ASM -DNDEBUG -D_FORTIFY_SOURCE=2 -DTERMIO -DPURIFY -D_GNU_SOURCE -DOPENSSL_NO_BUF_FREELISTS
   ```

   This command is then used to configure and build OpenSSL.  First, obtain the last 1.0 version of
   OpenSSL (installed on GE's OS, and openSUSE 15.2), at [this
   link](https://github.com/openssl/openssl/releases/tag/OpenSSL_1_1_1g).

   Then configure OpenSSL to install into Orchestra's environment (with the "--prefix=$SDKTOP/3p"
   option), and with the "Entropy Gathering Daemon" option activated (with the "enable-egd" flag
   added), i.e.:

   ```bash
      ./config --prefix=$SDKTOP/3p enable-egd -fPIC -pthread -m64 -Wa,--noexecstack -Wall -O3 -fmessage-length=0 -grecord-gcc-switches -O2 -Wall -fstack-protector-strong -funwind-tables -fasynchronous-unwind-tables -fstack-clash-protection -g -Wa,--noexecstack -fno-common -Wall -DOPENSSL_USE_NODELETE -DL_ENDIAN -DOPENSSL_PIC -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_BN_ASM_GF2m -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DKECCAK1600_ASM -DRC4_ASM -DMD5_ASM -DVPAES_ASM -DGHASH_ASM -DECP_NISTZ256_ASM -DX25519_ASM -DPOLY1305_ASM -DNDEBUG -D_FORTIFY_SOURCE=2 -DTERMIO -DPURIFY -D_GNU_SOURCE -DOPENSSL_NO_BUF_FREELISTS
   ```

   After the OpenSSL build configuration is complete, then running:

   ```bash
      make
      make install
   ```

   (from an environment with the appropriate system permissions) will build and install OpenSSL
   into Orchestra, so that ISMRMRD, and GE-to-ISMRMRD finds all required components, and
   successfully builds.  Continue with the steps previously outlined below, to build and install
   ISMRMRD, and GE-to-ISMRMRD.

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
    mkdir build
    cd build/
    cmake -D build4GE=ON -D CMAKE_INSTALL_PREFIX=$ISMRMRD_HOME ..
    make install
    cd ../
    ```

   If the situation is encountered where the system compilers and Boost version are "too far ahead"
   of how Orchestra's support libraries (which include Boost) were compiled, then it may be necessary
   to have the ISMRMRD build explicitly refer to Orchestra's Boost libraries, with a command like:

   ```bash
   cmake -D CMAKE_INSTALL_PREFIX=$ISMRMRD_HOME -D build4GE=TRUE -D Boost_NO_BOOST_CMAKE=TRUE -D Boost_NO_SYSTEM_PATHS=TRUE ..
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

1. If using the Gadgetron for reconstruction, please use a standard Gadgetron installation or Docker
   container.  The Gadgetron now requires Boost version 1.65 or newer, which is newer than that
   supplied with GE's latest Orchestra Linux SDK.  Therefore, Gadgetron currently cannot be built
   using components from GE's Orchestra Linux SDK, as was previously possible.

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
   ge2ismrmrd -v P21504_FSE.7
   ```

1. If customized conversion libraries and/or stylesheets are desired, the corresponding command will be:

   ```bash
   ge2ismrmrd -v -p NIH2dfastConverter -x $GE_TOOLS_HOME/share/ge-tools/config/default.xsl P21504_FSE.7
   ```

   The source code that enables this example is included with these tools. This example is a straightforward
   copy of the GenericConverter, but it shows how these classes can be inherited from and implemented.

1. Similarly, a typical command line to convert an example ScanArchive file using this library is:

   ```bash
   ge2ismrmrd -v -p GenericConverter -x $GE_TOOLS_HOME/share/ge-tools/config/default.xsl ScanArchive_FSE.h5
   ```

   Sample raw data files are now in the 'sampleData' directory.

