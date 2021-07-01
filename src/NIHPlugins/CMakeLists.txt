
set(NIH_LIB "g2i-NIH")
add_library (${NIH_LIB} SHARED 2dfastConverter.cpp 2dfastNoiseConverter.cpp epiConverter.cpp)

target_link_libraries(${NIH_LIB} ${BOOST_LIBRARIES} ${ISMRMRD_LIBRARIES} g2i)

install(TARGETS ${NIH_LIB} DESTINATION lib)
install(FILES 2dfastConverter.h 2dfastNoiseConverter.h epiConverter.h DESTINATION include/ge-tools)

