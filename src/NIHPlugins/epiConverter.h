
/** @file epiConverter.h */
#ifndef NIH_EPI_CONVERTER_H
#define NIH_EPI_CONVERTER_H

#include "ismrmrd/ismrmrd.h"

#include "GenericConverter.h"

class NIHepiConverter: public GEToIsmrmrd::GenericConverter
{
public:

   std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::Legacy::PfilePointer &pfile,
                                                      unsigned int view_num);

   std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::ScanArchivePointer &scanArchive,
                                                      unsigned int view_num);
};

#endif /* NIH_EPI_CONVERTER_H */

