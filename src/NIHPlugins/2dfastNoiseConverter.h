
/** @file 2dfastNoiseConverter.h */
#ifndef NIH_2DFAST_NOISE_CONVERTER_H
#define NIH_2DFAST_NOISE_CONVERTER_H

#include "ismrmrd/ismrmrd.h"

#include "GenericConverter.h"

class NIH2dfastNoiseConverter: public PfileToIsmrmrd::GenericConverter
{
public:
   std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
                                                     unsigned int acqMode);

   std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::ScanArchivePointer &scanArchive,
                                                     unsigned int acqMode);
};

#endif /* NIH_2DFAST_NOISE_CONVERTER_H */

