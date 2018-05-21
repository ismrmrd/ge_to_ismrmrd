
cmake_minimum_required (VERSION 2.8)
project(GE-TOOLS)

#Set the build type to Release if not specified
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif (NOT CMAKE_BUILD_TYPE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(LibXslt REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Ismrmrd REQUIRED)

find_package(Orchestra REQUIRED)
if (NOT ORCHESTRA_FOUND)
    message(WARNING "Orchestra SDK not found. Try setting $SDKTOP environment variable")
    return()
endif()

add_definitions(-std=c++11 ${ORCHESTRA_DEFINITIONS})

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif(APPLE)


# build C++ converter
add_subdirectory(src)

add_custom_command(
    OUTPUT tags
    COMMAND ctags -R --languages=C,+C++ ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_custom_target(ctags DEPENDS tags)

# For adding Gadgetron restruction chain configurations specific to this converter.
find_path(GADGETRON_HOME include/gadgetron/Gadget.h
    HINTS $ENV{GADGETRON_HOME} /usr/local/gadgetron /usr/gadgetron)
mark_as_advanced(GADGETRON_HOME)

install ( FILES         gtConfigs/gtReconExampleGE2D.xml
          DESTINATION   ${GADGETRON_HOME}/share/gadgetron/config
          COMPONENT     main )

