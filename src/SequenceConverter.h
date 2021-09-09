
/** @file SequenceConverter.h */
#ifndef SEQUENCE_CONVERTER_H
#define SEQUENCE_CONVERTER_H

#include <iostream>

// Orchestra
#include <Orchestra/Common/ArchiveHeader.h>
#include <Orchestra/Common/PrepData.h>
#include <Orchestra/Common/ImageCorners.h>
#include <Orchestra/Common/ScanArchive.h>
#include <Orchestra/Common/SliceInfoTable.h>

#include <Orchestra/Control/ProcessingControl.h>

#include <Orchestra/Legacy/Pfile.h>
#include <Orchestra/Legacy/DicomSeries.h>

#include <Orchestra/Acquisition/ControlPacket.h>
#include <Orchestra/Acquisition/ControlTypes.h>
#include <Orchestra/Acquisition/Core/ArchiveStorage.h>
#include <Orchestra/Acquisition/DataTypes.h>
#include <Orchestra/Acquisition/FrameControl.h>

#include <Dicom/Core/MR/Image.h>
#include <Dicom/Core/MR/ImageModule.h>
#include <Dicom/Core/MR/PrivateAcquisitionModule.h>
#include <Dicom/Core/Patient.h>
#include <Dicom/Core/PatientModule.h>
#include <Dicom/Core/PatientStudyModule.h>
#include <Dicom/Core/Equipment.h>
#include <Dicom/Core/EquipmentModule.h>
#include <Dicom/Core/ImagePlaneModule.h>
#include <Dicom/Core/UID.h>

// Trying to see if putting sequence-specific headers here will allow creation
// of sequence-specific control objects in the GERawConverter module, which
// should allow that module to put information about sequence specific objects
// into base XML header stream.

#include <Orchestra/Common/ReconPaths.h>
#include <Orchestra/Epi/LxControlSource.h>
#include <Orchestra/Epi/RowFlipParameters.h>
#include <Orchestra/Epi/RowFlipPlugin.h>
#include <Orchestra/Epi/PhaseCorrectionReferenceFile.h>
#include <Orchestra/Epi/SelfNavDynamicPhaseCorrectionPlugin.h>
#include <Orchestra/Epi/Diffusion/DynamicPhaseCorrectionManager.h>

using namespace MDArray;
using namespace GERecon;
using namespace GERecon::Epi;

// end of EPI-related "includes" ...



// ISMRMRD
#include "ismrmrd/ismrmrd.h"

namespace GEToIsmrmrd {

class SequenceConverter
{
public:
    SequenceConverter() { }
    ~SequenceConverter() { }

    /**
     * Create the ISMRMRD acquisitions corresponding to a given view in memory
     *
     * @param P-file or Orchestra file object
     * @param view_num View number
     * @returns vector of ISMRMRD::Acquisitions
     *
     * Pure virtual function templates
     */

    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
                                                              unsigned int view_num) = 0;

    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::ScanArchivePointer &scanArchive,
                                                              unsigned int view_num) = 0;
};

} // namespace GEToIsmrmrd

#endif /* SEQUENCE_CONVERTER_H */

