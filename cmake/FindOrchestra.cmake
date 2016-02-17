# - Find Orchestra SDK
#
#  ORCHESTRA_INCLUDE_DIR    - top-level include dir only
#  ORCHESTRA_INCLUDE_DIRS   - top-level include dir plus 3rd party Blitz dir
#  ORCHESTRA_LIBRARIES
#  ORCHESTRA_FOUND
#
# © 2015 Joseph Naegele

if(NOT ORCHESTRA_FOUND AND NOT ORCHESTRA_ROOT)
    find_path(ORCHESTRA_ROOT
        recon/Orchestra/Legacy/Pfile.h
        HINTS $ENV{SDKTOP})
    mark_as_advanced(ORCHESTRA_ROOT)
endif()

find_path(ORCHESTRA_INCLUDE_DIR Orchestra/Legacy/Pfile.h
    PATHS ${ORCHESTRA_ROOT} PATH_SUFFIXES recon)
list(APPEND ORCHESTRA_INCLUDE_DIRS ${ORCHESTRA_INCLUDE_DIR})

set(ORCHESTRA_Dicom_LIB_NAME GEDicom)
set(ORCHESTRA_Hdf5_LIB_NAME GEHdf5)
set(ORCHESTRA_Math_LIB_NAME GEMath)
set(ORCHESTRA_OrchestraArc_LIB_NAME OrchestraArc)
set(ORCHESTRA_OrchestraAsset_LIB_NAME OrchestraAsset)
set(ORCHESTRA_OrchestraCalibration3D_LIB_NAME OrchestraCalibration3D)
set(ORCHESTRA_OrchestraCalibrationCommon_LIB_NAME OrchestraCalibrationCommon)
set(ORCHESTRA_OrchestraCalibrationRehearsalPipeline_LIB_NAME OrchestraCalibrationRehearsalPipeline)
set(ORCHESTRA_OrchestraCartesian2D_LIB_NAME OrchestraCartesian2D)
set(ORCHESTRA_OrchestraCartesian3D_LIB_NAME OrchestraCartesian3D)
set(ORCHESTRA_OrchestraClariview_LIB_NAME OrchestraClariview)
set(ORCHESTRA_OrchestraCommon_LIB_NAME OrchestraCommon)
set(ORCHESTRA_OrchestraControl_LIB_NAME OrchestraControl)
set(ORCHESTRA_OrchestraCore_LIB_NAME OrchestraCore)
set(ORCHESTRA_OrchestraCrucial_LIB_NAME OrchestraCrucial)
set(ORCHESTRA_OrchestraEpi_LIB_NAME OrchestraEpi)
set(ORCHESTRA_OrchestraEpiDiffusion_LIB_NAME OrchestraEpiDiffusion)
set(ORCHESTRA_OrchestraEpiMultiPhase_LIB_NAME OrchestraEpiMultiPhase)
set(ORCHESTRA_OrchestraFlex_LIB_NAME OrchestraFlex)
set(ORCHESTRA_OrchestraGradwarp_LIB_NAME OrchestraGradwarp)
set(ORCHESTRA_OrchestraLegacy_LIB_NAME OrchestraLegacy)
set(ORCHESTRA_OrchestraPurePure1_LIB_NAME OrchestraPure1)
set(ORCHESTRA_OrchestraPurePure2_LIB_NAME OrchestraPure2)
set(ORCHESTRA_OrchestraScic_LIB_NAME OrchestraScic)
set(ORCHESTRA_OrchestraSpectroCommon_LIB_NAME OrchestraSpectroCommon)
set(ORCHESTRA_OrchestraSpectroMCSI_LIB_NAME OrchestraSpectroMCSI)
set(ORCHESTRA_OrchestraSpectroMCSILegacy_LIB_NAME OrchestraSpectroMCSILegacy)
set(ORCHESTRA_OrchestraSpectroMultiVoxel_LIB_NAME OrchestraSpectroMultiVoxel)
set(ORCHESTRA_OrchestraSpectroSingleVoxel_LIB_NAME OrchestraSpectroSingleVoxel)
set(ORCHESTRA_System_LIB_NAME System)

foreach(dir
        Orchestra/Cartesian2D
        Orchestra/Gradwarp
        Orchestra/Crucial
        Orchestra/Core
        Orchestra/Legacy
        Orchestra/Control
        Orchestra/Common
        Hdf5
        Dicom
        System

        #Orchestra/Arc
        #Orchestra/Asset
        #Orchestra/Calibration/3D
        #Orchestra/Calibration/Common
        #Orchestra/Calibration/Rehearsal/Pipeline
        #Orchestra/Cartesian3D
        #Orchestra/Clariview
        #Orchestra/Epi
        #Orchestra/Epi/Diffusion
        #Orchestra/Epi/MultiPhase
        #Orchestra/Flex
        #Orchestra/Pure/Pure1
        #Orchestra/Pure/Pure2
        #Orchestra/Scic
        #Orchestra/Spectro/Common
        #Orchestra/Spectro/MCSI
        #Orchestra/Spectro/MCSI/Legacy
        #Orchestra/Spectro/MultiVoxel
        #Orchestra/Spectro/SingleVoxel
        #Math
        )

    string(REPLACE "/" "" _name ${dir})
    find_library(ORCHESTRA_${_name}_LIBRARY
        ${ORCHESTRA_${_name}_LIB_NAME}
        PATHS ${ORCHESTRA_ROOT}/recon/${dir}
        PATH_SUFFIXES mac64 dev_linux_release64
        NO_DEFAULT_PATH)
    mark_as_advanced(ORCHESTRA_${_name}_LIBRARY)
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_${_name}_LIBRARY})
endforeach()

# Orchestra HDF5 include directory
find_path(ORCHESTRA_HDF5_INCLUDE_DIR hdf5.h
    PATHS ${ORCHESTRA_ROOT}/recon/3p
    PATH_SUFFIXES mac64/hdf5-1.8.12_mac64/include Linux/hdf5-1.8.12_dev_linux64/include
    NO_DEFAULT_PATH)
mark_as_advanced(${ORCHESTRA_HDF5_INCLUDE_DIR})
list(APPEND ORCHESTRA_INCLUDE_DIRS ${ORCHESTRA_HDF5_INCLUDE_DIR})

# Orchestra HDF5 libraries
foreach(lib h5tools hdf5_cpp hdf5)
    find_library(ORCHESTRA_HDF5_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_ROOT}/recon/3p
        PATH_SUFFIXES mac64/hdf5-1.8.12_mac64/lib Linux/hdf5-1.8.12_dev_linux64/lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_HDF5_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_HDF5_${lib}_LIBRARY})
endforeach()

# Orchestra DCMTK libraries
foreach(lib dcmdata dcmnet oflog ofstd)
    find_library(ORCHESTRA_DCMTK_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_ROOT}/recon/3p
        PATH_SUFFIXES mac64/dcmtk-3.6.1_20140617_mac64/lib Linux/dcmtk-3.6.0_dev_linux64/lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_DCMTK_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_DCMTK_${lib}_LIBRARY})
endforeach()

# Orchestra FFTW libraries
foreach(lib fftw3f fftw3)
    find_library(ORCHESTRA_FFTW_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_ROOT}/recon/3p
        PATH_SUFFIXES mac64/fftw-3.2.2_mac64/lib Linux/fftw-3.2.2_dev_linux64/lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_FFTW_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_FFTW_${lib}_LIBRARY})
endforeach()

# Orchestra Boost include directory
find_path(ORCHESTRA_BOOST_INCLUDE_DIR boost/version.hpp
    PATHS ${ORCHESTRA_ROOT}/recon/3p
    PATH_SUFFIXES mac64/boost_1_55_0_mac64/include Linux/boost_1_55_0_dev_linux64/include
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
        PATHS ${ORCHESTRA_ROOT}/recon/3p
        PATH_SUFFIXES mac64/boost_1_55_0_mac64/lib Linux/boost_1_55_0_dev_linux64/lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_BOOST_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_BOOST_${lib}_LIBRARY})
endforeach()

# Orchestra LAPACK libraries
foreach(lib lapack blas f2c)
    find_library(ORCHESTRA_LAPACK_${lib}_LIBRARY ${lib}
        PATHS ${ORCHESTRA_ROOT}/recon/3p
        PATH_SUFFIXES mac64/clapack-3.2.1_mac64/lib Linux/clapack-3.2.1_dev_linux64/lib
        NO_DEFAULT_PATH)
    mark_as_advanced(${ORCHESTRA_LAPACK_${lib}_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_LAPACK_${lib}_LIBRARY})
endforeach()

# Orchestra Blitz include dir
find_path(ORCHESTRA_BLITZ_INCLUDE_DIR blitz/blitz.h
    PATHS ${ORCHESTRA_ROOT}/recon/3p
    PATH_SUFFIXES mac64/blitz-0.10_mac64/include Linux/blitz-0.10_dev_linux64/include)
if(ORCHESTRA_BLITZ_INCLUDE_DIR)
    mark_as_advanced(${ORCHESTRA_BLITZ_INCLUDE_DIR})
    list(APPEND ORCHESTRA_INCLUDE_DIRS ${ORCHESTRA_BLITZ_INCLUDE_DIR})
endif()

# Orchestra Blitz library
find_library(ORCHESTRA_BLITZ_LIBRARY blitz
    PATHS ${ORCHESTRA_ROOT}/recon/3p
    PATH_SUFFIXES mac64/blitz-0.10_mac64/lib Linux/blitz-0.10_dev_linux64/lib
    NO_DEFAULT_PATH)
if(ORCHESTRA_BLITZ_LIBRARY)
    mark_as_advanced(${ORCHESTRA_BLITZ_LIBRARY})
    list(APPEND ORCHESTRA_LIBRARIES ${ORCHESTRA_BLITZ_LIBRARY})
endif()

#message("Orchestra Include Dir: ${ORCHESTRA_INCLUDE_DIR}")
#message("Orchestra Include Dirs: ${ORCHESTRA_INCLUDE_DIRS}")
#message("Orchestra Libraries: ${ORCHESTRA_LIBRARIES}")

list(APPEND ORCHESTRA_LIBRARIES pthread dl)

set(ORCHESTRA_DEFINITIONS -w -pthread -DGE_64BIT -DEXCLUDE_RSP_TYPE_DEFINES -D_GE_ESE_BUILD)

INCLUDE("FindPackageHandleStandardArgs")
FIND_PACKAGE_HANDLE_STANDARD_ARGS("Orchestra"
    "GE Orchestra SDK NOT FOUND. Try setting $SDKTOP environment variable"
    ORCHESTRA_INCLUDE_DIR ORCHESTRA_INCLUDE_DIRS ORCHESTRA_LIBRARIES ORCHESTRA_DEFINITIONS)

MARK_AS_ADVANCED(ORCHESTRA_INCLUDE_DIR ORCHESTRA_INCLUDE_DIRS ORCHESTRA_LIBRARIES)
