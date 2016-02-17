/** @file SequenceConverter.h */
#ifndef SEQUENCE_CONVERTER_H
#define SEQUENCE_CONVERTER_H

#include <iostream>

#include "Orchestra/Legacy/Pfile.h"

// ISMRMRD
#include "ismrmrd/ismrmrd.h"

namespace PfileToIsmrmrd {

/*
#define PLUGIN_DEBUG(v) \
    do { \
        cout << #v ": " << v << endl; \
    } while (0)
*/
#define PLUGIN_DEBUG(v)

enum { PLUGIN_FAILURE = -1, PLUGIN_SUCCESS = 1 };

class SequenceConverter
{
public:
    SequenceConverter() { }
    virtual ~SequenceConverter() { }

    /**
     * Create the ISMRMRD acquisitions corresponding to a given view in memory
     *
     * @param pfile Orchestra Pfile object
     * @param view_num View number
     * @returns vector of ISMRMRD::Acquisitions
     */
    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::Legacy::Pfile* pfile, unsigned int view_num) = 0;

};


// This MACRO goes in the Sequence header file
#define SEQUENCE_CONVERTER_DECLARE(SEQ)                     \
    SEQ () : PfileToIsmrmrd::SequenceConverter() {}

// This MACRO goes at the end of the Sequence source file
#define SEQUENCE_CONVERTER_FACTORY_DECLARE(SEQ)             \
                                                            \
extern "C" PfileToIsmrmrd::SequenceConverter * make_##SEQ ()                \
{                                                           \
    return new SEQ();                                       \
}                                                           \
                                                            \
extern "C" void destroy_##SEQ (PfileToIsmrmrd::SequenceConverter *s)        \
{                                                           \
    delete s;                                               \
}

} // namespace PfileToIsmrmrd

#endif /* SEQUENCE_CONVERTER_H */
