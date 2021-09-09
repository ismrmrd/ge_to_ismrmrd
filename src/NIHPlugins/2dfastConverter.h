
/** @file 2dfastConverter.h */
#ifndef NIH_2DFAST_CONVERTER_H
#define NIH_2DFAST_CONVERTER_H

#include "ismrmrd/ismrmrd.h"

#include "GenericConverter.h"

class NIH2dfastConverter: public GEToIsmrmrd::GenericConverter
{
public:
   NIH2dfastConverter() : GEToIsmrmrd::GenericConverter() {}
};

#endif /* NIH_2DFAST_CONVERTER_H */

