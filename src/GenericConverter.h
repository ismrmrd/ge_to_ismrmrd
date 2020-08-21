
/** @file GenericConverter.h */
#ifndef GENERIC_CONVERTER_H
#define GENERIC_CONVERTER_H

#include "SequenceConverter.h"

/** A 3-D vector representation */
struct geRawDataVector {
   float x;    /**< X-coordinate */
   float y;    /**< Y-coordinate */
   float z;    /**< Z-coordinate */
};
typedef struct geRawDataVector geRawDataVector_t;

/** A convenience structure used to obtain slice vectors for a given slice */
struct geRawDataSliceVectors {
   geRawDataVector_t center;      /**< Center coordinate */
   geRawDataVector_t read_dir;    /**< Readout direction vector */
   geRawDataVector_t phase_dir;   /**< Phase direction vector */
   geRawDataVector_t slice_dir;   /**< Slice direction vector */
};
typedef struct geRawDataSliceVectors geRawDataSliceVectors_t;

namespace PfileToIsmrmrd {

class GenericConverter: public SequenceConverter
{
public:
    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::Legacy::PfilePointer &pfile,
                                                               unsigned int view_num);

    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::ScanArchivePointer &scanArchivePtr,
                                                               unsigned int view_num);

    int                                       getSliceVectors (GERecon::Control::ProcessingControlPointer processingControl,
                                                               unsigned int sliceNumber, geRawDataSliceVectors_t* vecs);

protected:
    virtual int get_view_idx(GERecon::Control::ProcessingControlPointer processingControl,
                             unsigned int view_num, ISMRMRD::EncodingCounters &idx);
};

} // namespace PfileToIsmrmrd

#endif /* GENERIC_CONVERTER_H */

