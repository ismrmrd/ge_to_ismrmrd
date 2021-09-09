
include_directories(${LIBXML2_INCLUDE_DIR})

include_directories(
    ${ORCHESTRA_INCLUDE_DIRS}
    ${ISMRMRD_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${LIBXML2_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})

find_package(OpenSSL REQUIRED)

# build GE to ISMRMRD converter library and tool
set(G2I_LIB "g2i")
add_library(${G2I_LIB} SHARED
            GERawConverter.cpp
            GenericConverter.cpp
            NIHPlugins/2dfastConverter.cpp
            NIHPlugins/epiConverter.cpp
           )
target_link_libraries(${G2I_LIB}
    tls
    gomp
    ${ORCHESTRA_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    dl)
install(TARGETS ${G2I_LIB} DESTINATION lib)
install(FILES SequenceConverter.h
              GERawConverter.h
              GenericConverter.h
        DESTINATION include/ge-tools)

set(G2I_EXE "ge2ismrmrd")
add_executable(${G2I_EXE}
               main.cpp
              )
target_link_libraries(${G2I_EXE}
    ssl
    crypto
    ${G2I_LIB}
    ${ISMRMRD_LIBRARIES})
install(TARGETS ${G2I_EXE} DESTINATION bin)

install(DIRECTORY config/
        DESTINATION share/ge-tools/config)

add_subdirectory(NIHPlugins)

# API documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_TARGET "doc")
    set(DOXYGEN_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${DOXYGEN_TARGET}/html/index.html)
    set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYFILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # configure Doxyfile.in, generating Doxyfile
    configure_file(${DOXYFILE_IN} ${DOXYFILE} @ONLY)

    add_custom_command(
        OUTPUT ${DOXYGEN_OUTPUT}
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API Documentation with Doxygen..." VERBATIM
        DEPENDS ${DOXYFILE})

    add_custom_target(${DOXYGEN_TARGET} DEPENDS ${DOXYGEN_OUTPUT})
else(DOXYGEN_FOUND)
    message("Doxygen is needed to build ge-ismrmrd's API docs")
endif(DOXYGEN_FOUND)

