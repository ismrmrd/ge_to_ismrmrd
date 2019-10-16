
/** @file QGenericConverter.h */
#ifndef QGENERIC_CONVERTER_H
#define QGENERIC_CONVERTER_H

#include "SequenceConverter.h"

namespace PfileToIsmrmrd {

class QGenericConverter: public SequenceConverter
{
public:
    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::Legacy::PfilePointer &pfile,
                                                               unsigned int view_num);

    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::ScanArchivePointer &scanArchivePtr,
                                                               unsigned int view_num);

protected:
    virtual int get_view_idx(GERecon::Control::ProcessingControlPointer processingControl,
                             unsigned int view_num, ISMRMRD::EncodingCounters &idx);
};

} // namespace PfileToIsmrmrd

#endif /* QGENERIC_CONVERTER_H */

