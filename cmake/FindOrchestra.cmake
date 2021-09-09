
# - Find Orchestra SDK
#
#  ORCHESTRA_INCLUDE_DIR    - top-level include dir only
#  ORCHESTRA_INCLUDE_DIRS   - top-level include dir plus 3rd party Blitz dir
#  ORCHESTRA_LIBRARIES
#  ORCHESTRA_FOUND
#
# © 2015 Joseph Naegele
#
# updated in 2017 by V. R. to handle 2017 (1.7-1) Orchestra layout
#
# and in 2020 for Orchestra 1.8-1 layout

set (ORCHESTRA_TOPDIR $ENV{SDKTOP})

set (ORCHESTRA_INCLUDE_DIR ${ORCHESTRA_TOPDIR}/ $ENV{SDKTOP}/include/lx/include/)
set (ORCHESTRA_INCLUDE_DIRS $ENV{SDKTOP}/3p/ $ENV{SDKTOP}/include/recon/)

foreach(libs
         Dicom
         Hdf5
         Math
         Acquisition
         Arc
         Asset
         BiasCorrection
         Calibration3D
         CalibrationCommon
         Cartesian2D
         Cartesian3D
         Clariview
         Common
         Control
         Core
         Crucial
         Epi
         EpiDiffusion
         EpiDistortionCorrection
         EpiReferenceScan
         Flex
         Foundation
         FrameDownSampler
         Gradwarp
         Legacy
         MoCo
         Pure1
         Pure2
         Scic
         SpectroCommon
         SpectroMCSI
         SpectroMCSILegacy
         SpectroMultiVoxel
         SpectroSingleVoxel
         Spiral
         TestSupport
         ProcessingControl
         ProcessingFlow
         System
         SystemServicesImplementation
         SystemServicesInterface
         CalibrationMatchers
         MaestroModels
       )

    message("Finding library: lib${libs}.a")

    list(APPEND ORCHESTRA_LIBRARIES $ENV{SDKTOP}/lib/lib${libs}.a)
endforeach()

# Orchestra HDF5 include directory
find_path(ORCHESTRA_HDF5_INCLUDE_DIR hdf5.h
    PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/hdf5-1.8.12
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)
mark_as_advanced(${ORCHESTRA_HDF5_INCLUDE_DIR})
list(APPEND ORCHESTRA_INCLUDE_DIRS ${ORCHESTRA_HDF5_INCLUDE_DIR})

# Orchestra HDF5 libraries
foreach(lib h5tools hdf5_cpp hdf5 Hdf5)
    find_library(ORCHESTRA_HDF5_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_TOPDIR}   ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/hdf5-1.8.12
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_HDF5_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_HDF5_${lib}_LIBRARY})
endforeach()

# Orchestra DCMTK libraries
foreach(lib dcmdata dcmnet dcmtls oflog ofstd ssl crypto)
    find_library(ORCHESTRA_DCMTK_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/dcmtk-3.6.1_20140617
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_DCMTK_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_DCMTK_${lib}_LIBRARY})
endforeach()

# Orchestra FFTW libraries
foreach(lib fftw3f fftw3)
    find_library(ORCHESTRA_FFTW_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/fftw-3.2.2
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_FFTW_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_FFTW_${lib}_LIBRARY})
endforeach()

# Orchestra Boost include directory
find_path(ORCHESTRA_BOOST_INCLUDE_DIR boost/version.hpp
    PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/boost_1_63_0
    PATH_SUFFIXES include include/boost
    NO_DEFAULT_PATH)
if(ORCHESTRA_BOOST_INCLUDE_DIR)
    mark_as_advanced(${ORCHESTRA_BOOST_INCLUDE_DIR})
    list(APPEND ORCHESTRA_INCLUDE_DIRS ${ORCHESTRA_BOOST_INCLUDE_DIR})
else()
    message("Orchestra Boost include dir not found")
endif()

# Orchestra Boost libraries
foreach(lib date_time program_options filesystem regex serialization wserialization thread system)
    find_library(ORCHESTRA_BOOST_${lib}_LIBRARY "boost_${lib}"
        PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/boost_1_63_0
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_BOOST_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_BOOST_${lib}_LIBRARY})
endforeach()

# Orchestra LAPACK libraries
foreach(lib lapack blas f2c)
    find_library(ORCHESTRA_LAPACK_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/clapack-3.2.1
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_LAPACK_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_LAPACK_${lib}_LIBRARY})
endforeach()

# Orchestra Blitz include dir
find_path(ORCHESTRA_BLITZ_INCLUDE_DIR blitz/blitz.h
    PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/blitz-0.10
    PATH_SUFFIXES include)
if(ORCHESTRA_BLITZ_INCLUDE_DIR)
    mark_as_advanced(${ORCHESTRA_BLITZ_INCLUDE_DIR})
    list(APPEND ORCHESTRA_INCLUDE_DIRS ${ORCHESTRA_BLITZ_INCLUDE_DIR})
endif()

# Orchestra Blitz library
find_library(ORCHESTRA_BLITZ_LIBRARY blitz
    PATHS ${ORCHESTRA_TOPDIR}/3p   ${ORCHESTRA_TOPDIR}/include/recon/3p/mac/blitz-0.10
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
if(ORCHESTRA_BLITZ_LIBRARY)
    mark_as_advanced(${ORCHESTRA_BLITZ_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_BLITZ_LIBRARY})
endif()

include_directories (${ORCHESTRA_INCLUDE_DIR} ${ORCHESTRA_INCLUDE_DIRS})

message("Orchestra Install Root: $ENV{SDKTOP}")
message("Orchestra Top Dir:      ${ORCHESTRA_TOPDIR}")
message("Orchestra Include Dir:  ${ORCHESTRA_INCLUDE_DIR}")
message("Orchestra Include Dirs: ${ORCHESTRA_INCLUDE_DIRS}")
message("Orchestra Libraries:    ${ORCHESTRA_LIBRARIES}")

list(APPEND ORCHESTRA_LIBRARIES pthread z dl)

set(ORCHESTRA_DEFINITIONS -w -pthread -DGE_64BIT -DEXCLUDE_RSP_TYPE_DEFINES -D_GE_ESE_BUILD)

INCLUDE("FindPackageHandleStandardArgs")
FIND_PACKAGE_HANDLE_STANDARD_ARGS("Orchestra"
    "GE Orchestra SDK NOT FOUND. Try setting $SDKTOP environment variable"
    ORCHESTRA_INCLUDE_DIR ORCHESTRA_INCLUDE_DIRS ORCHESTRA_LIBRARIES ORCHESTRA_DEFINITIONS)

MARK_AS_ADVANCED(ORCHESTRA_INCLUDE_DIR ORCHESTRA_INCLUDE_DIRS ORCHESTRA_LIBRARIES)

