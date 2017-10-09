
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

