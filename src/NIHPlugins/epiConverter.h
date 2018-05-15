
/** @file 2dfastConverter.h */
#ifndef NIH_EPI_CONVERTER_H
#define NIH_EPI_CONVERTER_H

#include <Orchestra/Common/ReconPaths.h>
#include <Orchestra/Epi/LxControlSource.h>
#include <Orchestra/Epi/RowFlipParameters.h>
#include <Orchestra/Epi/RowFlipPlugin.h>
#include <Orchestra/Epi/PhaseCorrectionReferenceFile.h>
#include <Orchestra/Epi/SelfNavDynamicPhaseCorrectionPlugin.h>
#include <Orchestra/Epi/Diffusion/DynamicPhaseCorrectionManager.h>

#include "ismrmrd/ismrmrd.h"

#include "GenericConverter.h"

using namespace MDArray;
using namespace GERecon;
using namespace GERecon::Epi;

class NIHepiConverter: public PfileToIsmrmrd::GenericConverter
{
public:

   virtual std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::Legacy::Pfile* pfile,
                                                               unsigned int view_num);

   virtual std::vector<ISMRMRD::Acquisition> getAcquisitions (GERecon::ScanArchive* scanArchive,
                                                               unsigned int view_num);

};

#endif /* NIH_EPI_CONVERTER_H */

