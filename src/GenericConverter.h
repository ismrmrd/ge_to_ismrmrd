
/** @file GenericConverter.h */
#ifndef GENERIC_CONVERTER_H
#define GENERIC_CONVERTER_H

#include "SequenceConverter.h"

namespace PfileToIsmrmrd {

class GenericConverter: public SequenceConverter
{
public:
    SEQUENCE_CONVERTER_DECLARE(GenericConverter)
    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions(
               GERecon::Legacy::Pfile* pfile, unsigned int acq_mode);

protected:
    virtual int get_view_idx(GERecon::Legacy::Pfile *pfile,
               unsigned int view_num, ISMRMRD::EncodingCounters &idx);
};

} // namespace PfileToIsmrmrd

#endif /* GENERIC_CONVERTER_H */

