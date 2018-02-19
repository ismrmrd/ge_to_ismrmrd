
/** @file 2dfastConverter.h */
#ifndef NIH_EPI_CONVERTER_H
#define NIH_EPI_CONVERTER_H

#include "ismrmrd/ismrmrd.h"

#include "GenericConverter.h"

class NIHepiConverter: public PfileToIsmrmrd::GenericConverter
{
public:
   NIHepiConverter() : PfileToIsmrmrd::GenericConverter() {}
};

#endif /* NIH_EPI_CONVERTER_H */

