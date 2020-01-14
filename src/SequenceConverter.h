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
#include <Orchestra/Acquisition/CartesianFrameCommand.h>

#include <Dicom/MR/Image.h>
#include <Dicom/MR/ImageModule.h>
#include <Dicom/MR/PrivateAcquisitionModule.h>
#include <Dicom/Patient.h>
#include <Dicom/PatientModule.h>
#include <Dicom/PatientStudyModule.h>
#include <Dicom/Equipment.h>
#include <Dicom/EquipmentModule.h>
#include <Dicom/ImagePlaneModule.h>
#include <Dicom/UID.h>

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
     *
     * Pure virtual function templates
     */

    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
                                                              unsigned int view_num) = 0;

    virtual std::vector<ISMRMRD::Acquisition> getAcquisitions(GERecon::ScanArchivePointer &scanArchive,
                                                              unsigned int view_num) = 0;
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
