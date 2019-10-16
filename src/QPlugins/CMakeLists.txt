
set(QBIO_LIB "p2i-Q")
add_library (${QBIO_LIB} SHARED QGenericConverter.cpp)

target_link_libraries(${QBIO_LIB} ${BOOST_LIBRARIES} ${ISMRMRD_LIBRARIES} p2i p2i-generic)

install(TARGETS ${QBIO_LIB} DESTINATION lib)
install(FILES QGenericConverter.h DESTINATION include/ge-tools)

