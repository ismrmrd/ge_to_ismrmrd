/** @file GERawConverter.cpp */
#include <iostream>
#include <stdexcept>

#include <libxml/xmlschemas.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

// Local
#include "GERawConverter.h"
#include "XMLWriter.h"
#include "ge_tools_path.h"


namespace GEToIsmrmrd {

const std::string g_schema = "\
<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>                \
<xs:schema xmlns=\"https://github.com/nih-fmrif/GEISMRMRD\"                 \
    xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"                           \
    elementFormDefault=\"qualified\"                                        \
    targetNamespace=\"https://github.com/nih-fmrif/GEISMRMRD\">             \
    <xs:element name=\"conversionConfiguration\">                           \
        <xs:complexType>                                                    \
            <xs:sequence>                                                   \
                <xs:element maxOccurs=\"unbounded\" minOccurs=\"1\"         \
                    name=\"sequenceMapping\" type=\"sequenceMappingType\"/> \
            </xs:sequence>                                                  \
        </xs:complexType>                                                   \
    </xs:element>                                                           \
    <xs:complexType name=\"sequenceMappingType\">                           \
        <xs:all>                                                            \
            <xs:element name=\"psdname\" type=\"xs:string\"/>               \
            <xs:element name=\"libraryPath\" type=\"xs:string\"/>           \
            <xs:element name=\"className\" type=\"xs:string\"/>             \
            <xs:element name=\"stylesheet\" type=\"xs:string\"/>            \
            <xs:element name=\"reconConfigName\" type=\"xs:string\"/>       \
        </xs:all>                                                           \
    </xs:complexType>                                                       \
</xs:schema>";

/**
 * Creates a GERawConverter from an ifstream of the raw data file header
 *
 * @param fp raw FILE pointer to raw data file
 * @throws std::runtime_error if raw data file cannot be read
 */
GERawConverter::GERawConverter(const std::string& rawFilePath, const std::string& classname, bool logging)
    : log_(logging)
{
   psdname_ = ""; // TODO: find PSD Name in Orchestra Pfile class
   log_ << "PSDName: " << psdname_ << std::endl;

   // Use Orchestra to figure out if P-File or ScanArchive
   if (GERecon::ScanArchive::IsArchiveFilePath(rawFilePath))
   {
      scanArchive_ = GERecon::ScanArchive::Create(rawFilePath, GESystem::Archive::LoadMode);
      lxData_ = boost::dynamic_pointer_cast<GERecon::Legacy::LxDownloadData>(scanArchive_->LoadDownloadData());

      boost::shared_ptr<GERecon::Legacy::LxControlSource> const controlSource = boost::make_shared<GERecon::Legacy::LxControlSource>(lxData_);
      processingControl_ = controlSource->CreateOrchestraProcessingControl();

      rawObjectType_ = SCAN_ARCHIVE_RAW_TYPE;
   }
   else
   {
      pfile_ = GERecon::Legacy::Pfile::Create(rawFilePath,
                                              GERecon::Legacy::Pfile::AllAvailableAcquisitions,
                                              GERecon::AnonymizationPolicy(GERecon::AnonymizationPolicy::None));

      lxData_ = pfile_->DownloadData();
      processingControl_ = pfile_->CreateOrchestraProcessingControl();

      rawObjectType_ = PFILE_RAW_TYPE;
   }

   if (!classname.compare("GenericConverter"))
   {
      converter_ = std::shared_ptr<SequenceConverter>(new GenericConverter());
   }
   else if (!classname.compare("NIH2dfastConverter"))
   {
      converter_ = std::shared_ptr<SequenceConverter>(new NIH2dfastConverter());
   }
   else if (!classname.compare("NIHepiConverter"))
   {
      converter_ = std::shared_ptr<SequenceConverter>(new NIHepiConverter());
   }
   else
   {
      std::cerr << "Plugin class name: " << classname << " not implemented. Exiting..." << std::endl;

      exit(EXIT_FAILURE);
   }

   // Testing dumping of raw file header as XML.
   // processingControl_->SaveAsXml("rawHeader.xml");  // As of Orchestra 1.8-1, this is causing a crash, with
                                                    // an incomplete file written.
}

void GERawConverter::useStylesheetFilename(const std::string& filename)
{
    log_ << "Loading stylesheet: " << filename << std::endl;
    std::ifstream stream(filename.c_str(), std::ios::binary);
    useStylesheetStream(stream);
}

void GERawConverter::useStylesheetStream(std::ifstream& stream)
{
    stream.seekg(0, std::ios::beg);

    std::string sheet((std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>());
    useStylesheetString(sheet);
}

void GERawConverter::useStylesheetString(const std::string& sheet)
{
    stylesheet_ = sheet;
}

/**
 * Converts the XSD ISMRMRD XML header object into a C++ string
 *
 * @returns string represenatation of ISMRMRD XML header
 * @throws std::runtime_error
 */
std::string GERawConverter::getIsmrmrdXMLHeader()
{
    if (stylesheet_.size() == 0) {
        throw std::runtime_error("No stylesheet configured");
    }

    std::string ge_raw_file_header(ge_header_to_xml(lxData_, processingControl_));

    // DEBUG: std::cout << "Converted header as XML string is: " << ge_raw_file_header << std::endl;

    xmlSubstituteEntitiesDefault(1);
    xmlLoadExtDtdDefaultValue = 1;

    // Normal pointer here because the xsltStylesheet takes ownership
    xmlDocPtr stylesheet_doc = xmlParseMemory(stylesheet_.c_str(), stylesheet_.size());
    if (NULL == stylesheet_doc) {
        throw std::runtime_error("Failed to parse stylesheet");
    }

    std::shared_ptr<xsltStylesheet> sheet = std::shared_ptr<xsltStylesheet>(
            xsltParseStylesheetDoc(stylesheet_doc), xsltFreeStylesheet);
    if (!sheet) {
        xmlFreeDoc(stylesheet_doc);
        throw std::runtime_error("Failed to parse stylesheet");
    }

    std::shared_ptr<xmlDoc> pfile_doc = std::shared_ptr<xmlDoc>(
            xmlParseMemory(ge_raw_file_header.c_str(), ge_raw_file_header.size()), xmlFreeDoc);
    if (!pfile_doc) {
        throw std::runtime_error("Failed to parse P-File XML");
    }

    log_ << "Applying stylesheet" << std::endl;
    const char *params[1] = { NULL };
    std::shared_ptr<xmlDoc> result = std::shared_ptr<xmlDoc>(
            xsltApplyStylesheet(sheet.get(), pfile_doc.get(), params), xmlFreeDoc);
    if (!result) {
        throw std::runtime_error("Failed to apply stylesheet");
    }

    xmlChar* output = NULL;
    int len = 0;
    if (xsltSaveResultToString(&output, &len, result.get(), sheet.get()) < 0) {
        throw std::runtime_error("Failed to save converted doc to string");
    }

    std::string ismrmrd_header((char*)output, len);
    xmlFree(output);
    return ismrmrd_header;
}


/**
 * Gets the acquisitions corresponding to a view in memory.
 *
 * @param view_num View number to get
 * @param vacq Vector of acquisitions
 * @throws std::runtime_error { if plugin fails to copy the data }
 */
std::vector<ISMRMRD::Acquisition> GERawConverter::getAcquisitions(unsigned int view_num)
{
   if (rawObjectType_ == SCAN_ARCHIVE_RAW_TYPE)
   {
      return converter_->getAcquisitions(scanArchive_, view_num);
   }
   else
   {
      return converter_->getAcquisitions(pfile_, view_num);
   }
}

/**
 * Gets the extra field "reconConfig" from the
 * ge-ismrmrd XML configuration. This can be used to
 * add this library to a Gadgetron client
 */
std::string GERawConverter::getReconConfigName(void)
{
    return std::string(recon_config_);
}

std::string GERawConverter::ge_header_to_xml(GERecon::Legacy::LxDownloadDataPointer lxData,
                                             GERecon::Control::ProcessingControlPointer processingControl)
{
    // DEBUG: std::cerr << "Starting conversion of raw file header to XML string" << std::endl;

    XMLWriter writer;

    writer.startDocument();

    writer.startElement("Header");

    writer.addBooleanElement("is3DAcquisition",    processingControl->Value<bool>("Is3DAcquisition"));
    writer.addBooleanElement("isCalibration",      lxData->IsCalibration());
    writer.addBooleanElement("isAssetCalibration", processingControl->Value<bool>("AssetCalibration"));
    writer.addBooleanElement("isPureCalibration",  processingControl->Value<bool>("PureCalibration"));
    writer.addBooleanElement("isArc",              lxData->IsArc());
    writer.addBooleanElement("isEpi",              lxData->IsEpi());
    writer.addBooleanElement("isFMRI",             lxData->IsFunctionalMri());
    writer.addBooleanElement("isEpiBottomUp",      lxData->IsBottomUpEpi());
    writer.addBooleanElement("isEpiTopDown",       lxData->IsTopDownEpi());
    writer.addBooleanElement("isEpiRefScan",       lxData->IsEpiRefScan());
    writer.addBooleanElement("isEpiRefless",       lxData->IsReflessEPI());
    writer.addBooleanElement("isEpiRampsampled",   processingControl->Value<bool>("RampSamplingEnabled"));
    writer.addBooleanElement("isEpiDiffusion",     lxData->IsDiffusionEpi());
    writer.addBooleanElement("isEpiMultiPhase",    lxData->IsMultiPhaseEpi());
    writer.addBooleanElement("isPropeller",        lxData->IsPropeller());
    writer.addBooleanElement("isRadial3D",         lxData->IsRadial3D());
    writer.addBooleanElement("isSpiral",           lxData->IsSpiral());
    // writer.addBooleanElement("isCine",             lxData->IsCine());
    writer.addBooleanElement("isCine",             processingControl->Value<bool>("CineData"));
    writer.addBooleanElement("isShimData",         processingControl->Value<bool>("ShimData"));
    writer.addBooleanElement("isGrassData",        processingControl->Value<bool>("GrassData"));
    // writer.addBooleanElement("is3DASL",            processingControl->Value<bool>("Is3DASL"));

    writer.formatElement("SliceCount", "%d",       processingControl->Value<int>("NumSlices"));
    writer.formatElement("ChannelCount", "%d",     processingControl->Value<int>("NumChannels"));
    writer.formatElement("OtherUID", "%s",         GEDicom::UID::Create(GEDicom::UID::OtherUID).c_str());

    GERecon::Legacy::DicomSeries legacySeries(lxData);
    GEDicom::SeriesPointer series = legacySeries.Series();
    GEDicom::SeriesModulePointer seriesModule = series->GeneralModule();
    writer.startElement("Series");
    // writer.formatElement("Number", "%d",           lxData->SeriesNumber());
    writer.formatElement("Number", "%d",           processingControl->Value<int>("SeriesNumber"));
    writer.formatElement("UID", "%s",              seriesModule->UID().c_str());
    writer.formatElement("Description", "%s",      seriesModule->SeriesDescription().c_str());
    // writer.formatElement("Modality", "%s",         seriesModule->Modality());
    writer.formatElement("Laterality", "%s",       seriesModule->Laterality().c_str());
    writer.formatElement("Date", "%s",             seriesModule->Date().c_str());
    writer.formatElement("Time", "%s",             seriesModule->Time().c_str());
    writer.formatElement("ProtocolName", "%s",     seriesModule->ProtocolName().c_str());
    writer.formatElement("OperatorName", "%s",     seriesModule->OperatorName().c_str());
    writer.formatElement("PpsDescription", "%s",   seriesModule->PpsDescription().c_str());
    // writer.formatElement("PatientEntry", "%s",     seriesModule->Entry());
    // writer.formatElement("PatientOrientation", "%s", seriesModule->Orientation());
    writer.endElement();

    GEDicom::StudyPointer study = series->Study();
    GEDicom::StudyModulePointer studyModule = study->GeneralModule();
    writer.startElement("Study");
    // writer.formatElement("Number", "%d",           studyModule->StudyNumber());
    // writer.formatElement("Number", "%d",           lxData->ExamNumber()); // seems to be lxData equivalent
    writer.formatElement("Number", "%u",           processingControl->Value<int>("ExamNumber"));
    writer.formatElement("UID", "%s",              studyModule->UID().c_str());
    writer.formatElement("Description", "%s",      studyModule->StudyDescription().c_str());
    writer.formatElement("Date", "%s",             studyModule->Date().c_str());
    writer.formatElement("Time", "%s",             studyModule->Time().c_str());
    writer.formatElement("ReferringPhysician", "%s",  studyModule->ReferringPhysician().c_str());
    writer.formatElement("AccessionNumber", "%s",  studyModule->AccessionNumber().c_str());
    writer.formatElement("ReadingPhysician", "%s", studyModule->ReadingPhysician().c_str());
    writer.endElement();

    GEDicom::PatientStudyModulePointer patientStudyModule = study->PatientStudyModule();
    GEDicom::PatientPointer patient = study->Patient();
    GEDicom::PatientModulePointer patientModule = patient->GeneralModule();
    writer.startElement("Patient");
    writer.formatElement("Name", "%s",             patientModule->Name().c_str());
    writer.formatElement("ID", "%s",               patientModule->ID().c_str());
    writer.formatElement("Birthdate", "%s",        patientModule->Birthdate().c_str());
    writer.formatElement("Gender", "%s",           patientModule->Gender().c_str());
    writer.formatElement("Age", "%s",              patientStudyModule->Age().c_str());
    writer.formatElement("Weight", "%s",           patientStudyModule->Weight().c_str());
    writer.formatElement("History", "%s",          patientStudyModule->History().c_str());
    writer.endElement();

    GEDicom::EquipmentPointer equipment = series->Equipment();
    GEDicom::EquipmentModulePointer equipmentModule = equipment->GeneralModule();
    writer.startElement("Equipment");
    writer.formatElement("Manufacturer", "%s",     equipmentModule->Manufacturer().c_str());
    writer.formatElement("Institution", "%s",      equipmentModule->Institution().c_str());
    writer.formatElement("Station", "%s",          equipmentModule->Station().c_str());
    writer.formatElement("ManufacturerModel", "%s",   equipmentModule->ManufacturerModel().c_str());
    writer.formatElement("DeviceSerialNumber", "%s",  equipmentModule->DeviceSerialNumber().c_str());
    writer.formatElement("UID", "%s",              GEDicom::UID::Create(GEDicom::UID::Equipment).c_str());
    writer.formatElement("SoftwareVersion", "%s",  equipmentModule->SoftwareVersion().c_str());
    writer.formatElement("PpsPerformedStation", "%s", equipmentModule->PpsPerformedStation().c_str());
    writer.formatElement("PpsPerformedLocation", "%s",equipmentModule->PpsPerformedLocation().c_str());
    writer.endElement();

    writer.formatElement("AcquiredXRes", "%d",     processingControl->Value<int>("AcquiredXRes"));
    writer.formatElement("AcquiredYRes", "%d",     processingControl->Value<int>("AcquiredYRes"));
    writer.formatElement("AcquiredZRes", "%d",     processingControl->Value<int>("AcquiredZRes"));

    writer.addBooleanElement("EvenEchoFrequencyFlip", processingControl->Value<bool>("EvenEchoFrequencyFlip"));
    writer.addBooleanElement("OddEchoFrequencyFlip",  processingControl->Value<bool>("OddEchoFrequencyFlip"));
    writer.addBooleanElement("EvenEchoPhaseFlip",  processingControl->Value<bool>("EvenEchoPhaseFlip"));
    writer.addBooleanElement("OddEchoPhaseFlip",   processingControl->Value<bool>("OddEchoPhaseFlip"));
    writer.addBooleanElement("ChoppedData",        processingControl->Value<bool>("ChoppedData"));
    writer.addBooleanElement("HalfEcho",           processingControl->Value<bool>("HalfEcho"));
    writer.formatElement("RawNex", "%u",           processingControl->Value<unsigned int>("RawNex"));
    writer.addBooleanElement("HalfNex",            processingControl->Value<bool>("HalfNex"));
    writer.addBooleanElement("ThreeQuarterNexData",   processingControl->Value<bool>("ThreeQuarterNexData"));
    writer.addBooleanElement("NoFrequencyWrapData",   processingControl->Value<bool>("NoFrequencyWrapData"));
    writer.addBooleanElement("NoPhaseWrapData",    processingControl->Value<bool>("NoPhaseWrapData"));
    writer.addBooleanElement("OverscanData",       processingControl->Value<bool>("OverscanData"));

    writer.formatElement("sequenceNumber",  "%d",  lxData->SequenceNumber());
    writer.formatElement("seriesPulseSeq",  "%d",  lxData->SeriesPulseSequence());
    writer.formatElement("scanType",        "%s",  lxData->ScanType().c_str());
    writer.formatElement("seriesDscrption", "%s",  lxData->SeriesDescription().c_str());

    std::cout << "Coverting series with description: " <<   lxData->SeriesDescription().c_str() << std::endl;
    std::cout << "Patient entry: "    << processingControl->Value<int>("PatientEntry")    << std::endl;
    std::cout << "Patient position: " << processingControl->Value<int>("PatientPosition") << std::endl;

    writer.formatElement("NumBaselineViews", "%d", processingControl->Value<int>("NumBaselineViews"));
    writer.formatElement("NumVolumes", "%d",       processingControl->Value<int>("NumVolumes"));
    writer.formatElement("NumEchoes", "%d",        processingControl->Value<int>("NumEchoes"));
    writer.formatElement("NumAcquisitions", "%d",  processingControl->Value<int>("NumAcquisitions"));
    writer.formatElement("DataSampleSize", "%d",   processingControl->Value<int>("DataSampleSize")); // in bytes
                                                                                                     // nacq_points = ncoils * frame_size

    // std::string patientPosition = GERecon::PatientPositionAsString(static_cast<GERecon::PatientPosition>(processingControl->Value<int>("PatientPosition")));
    // writer.formatElement("PatientPositionStr", "%s",  patientPosition.c_str());
    writer.formatElement("PatientPosition", "%d",  static_cast<GERecon::PatientPosition>(processingControl->Value<int>("PatientPosition")));
    writer.formatElement("PatientEntry", "%d",     processingControl->Value<int>("PatientEntry"));

    writer.formatElement("ScanCenter", "%f",       processingControl->Value<float>("ScanCenter"));
    writer.formatElement("Landmark", "%f",         processingControl->Value<float>("Landmark"));
    writer.formatElement("CoilConfigUID", "%u",    processingControl->Value<unsigned int>("CoilConfigUID"));
    try {
        // Attempt to use `int` for RawPassSize
        writer.formatElement("RawPassSize", "%llu", processingControl->Value<int>("RawPassSize"));
    } catch (const std::exception& e) {
        std::cerr << "Failed to retrieve RawPassSize as int: " << e.what() << ". Trying unsigned long long instead." << std::endl;
        try {
            // Fallback to `unsigned long long` if the first attempt fails
            writer.formatElement("RawPassSize", "%llu", processingControl->Value<unsigned long long>("RawPassSize"));
        } catch (const std::exception& e) {
            // If both attempts fail, set a default value and continue
            std::cerr << "Failed to retrieve RawPassSize as unsigned long long: " << e.what() << ". Using default value 0." << std::endl;
            writer.formatElement("RawPassSize", "%llu", 0ULL); // Default value
        }
    }

    // ReconstructionParameters
    writer.addBooleanElement("CreateMagnitudeImages", processingControl->Value<bool>("CreateMagnitudeImages"));
    writer.addBooleanElement("CreatePhaseImages",  processingControl->Value<bool>("CreatePhaseImages"));

    writer.formatElement("TransformXRes", "%d",    processingControl->Value<int>("TransformXRes"));
    writer.formatElement("TransformYRes", "%d",    processingControl->Value<int>("TransformYRes"));
    writer.formatElement("TransformZRes", "%d",    processingControl->Value<int>("TransformZRes"));

    writer.addBooleanElement("ChopX",              processingControl->Value<bool>("ChopX"));
    writer.addBooleanElement("ChopY",              processingControl->Value<bool>("ChopY"));
    writer.addBooleanElement("ChopZ",              processingControl->Value<bool>("ChopZ"));

    // GERecon::PrepData prepData(lxData);
    // GERecon::ArchiveHeader archiveHeader("ScanArchive", prepData);
    // DEBUG: archiveHeader.Print(std::cout); // Does not seem to currently work as expected

    const GERecon::SliceInfoTable sliceTable = processingControl->ValueStrict<GERecon::SliceInfoTable>("SliceTable");

    auto imageCorners = GERecon::ImageCorners(sliceTable.AcquiredSliceCorners(0),
					      sliceTable.SliceOrientation(0));
    auto grayscaleImage = GEDicom::GrayscaleImage(128, 128);
    auto dicomImage = GERecon::Legacy::DicomImage(grayscaleImage, 0, imageCorners, series, *lxData);
    auto imageModule = dicomImage.ImageModule();
    // auto privateIdentityModule = dicomImage.PrivateIdentityModule();

    writer.startElement("Image");
    writer.formatElement("EchoTime", "%s",         imageModule->EchoTime().c_str());
    writer.formatElement("RepetitionTime", "%s",   imageModule->RepetitionTime().c_str());
    writer.formatElement("InversionTime", "%s",    imageModule->InversionTime().c_str());
    writer.formatElement("ImageType", "%s",        imageModule->ImageType().c_str());
    writer.formatElement("ScanSequence", "%s",     imageModule->ScanSequence().c_str());
    writer.formatElement("SequenceVariant", "%s",  imageModule->SequenceVariant().c_str());
    writer.formatElement("ScanOptions", "%s",      imageModule->ScanOptions().c_str());
    writer.formatElement("AcquisitionType", "%d",  imageModule->AcqType());
    writer.formatElement("PhaseEncodeDirection", "%d",   imageModule->PhaseEncodeDirection());
    writer.formatElement("ImagingFrequency", "%s", imageModule->ImagingFrequency().c_str());
    writer.formatElement("MagneticFieldStrength", "%s",  imageModule->MagneticFieldStrength().c_str());
    writer.formatElement("SliceSpacing", "%s",     imageModule->SliceSpacing().c_str());
    writer.formatElement("FlipAngle", "%s",        imageModule->FlipAngle().c_str());
    writer.formatElement("EchoTrainLength", "%s",  imageModule->EchoTrainLength().c_str());
    // TODO: map SliceOrder to a string
    // std::string sliceOrder = GERecon::SliceOrderAsString(processingControl->ReconstructionParameters::SliceOrder());
    // writer.formatElement("SliceOrder", "%s",       sliceOrder.c_str());

    // Image Parameters
    writer.formatElement("ImageXRes", "%d",        processingControl->Value<int>("ImageXRes"));
    writer.formatElement("ImageYRes", "%d",        processingControl->Value<int>("ImageYRes"));

    auto imageModuleBase = dicomImage.ImageModuleBase();
    writer.formatElement("AcquisitionDate", "%s",  imageModuleBase->AcquisitionDate().c_str());
    writer.formatElement("AcquisitionTime", "%s",  imageModuleBase->AcquisitionTime().c_str());
    writer.formatElement("ImageDate", "%s",        imageModuleBase->ImageDate().c_str());
    writer.formatElement("ImageTime", "%s",        imageModuleBase->ImageTime().c_str());

    auto imagePlaneModule = dicomImage.ImagePlaneModule();
    writer.formatElement("ImageOrientation", "%s", imagePlaneModule->ImageOrientation().c_str());
    writer.formatElement("ImagePosition", "%s",    imagePlaneModule->ImagePosition().c_str());
    writer.formatElement("SliceThickness", "%f",   imagePlaneModule->SliceThickness());
    writer.formatElement("SliceLocation", "%f",    imagePlaneModule->SliceLocation());
    writer.formatElement("PixelSizeX", "%f",       imagePlaneModule->PixelSizeX());
    writer.formatElement("PixelSizeY", "%f",       imagePlaneModule->PixelSizeY());

    auto privateAcquisitionModule = dicomImage.PrivateAcquisitionModule();
    writer.formatElement("SecondEcho", "%s",       privateAcquisitionModule->SecondEcho().c_str());

    // std::cout << "Table position: " << privateAcquisitionModule->TableDelta() << std::endl; // always seems to be 0.000 - so not sure if useful

    writer.endElement();

    writer.startElement("UserVariables");
    writer.formatElement("rdb_hdr_user0",  "%d",   processingControl->Value<int>("UserValue0"));
    writer.formatElement("rdb_hdr_user1",  "%d",   processingControl->Value<int>("UserValue1"));
    writer.formatElement("rdb_hdr_user2",  "%d",   processingControl->Value<int>("UserValue2"));
    writer.formatElement("rdb_hdr_user3",  "%d",   processingControl->Value<int>("UserValue3"));
    writer.formatElement("rdb_hdr_user4",  "%d",   processingControl->Value<int>("UserValue4"));
    writer.formatElement("rdb_hdr_user5",  "%d",   processingControl->Value<int>("UserValue5"));
    writer.formatElement("rdb_hdr_user6",  "%d",   processingControl->Value<int>("UserValue6"));
    writer.formatElement("rdb_hdr_user7",  "%d",   processingControl->Value<int>("UserValue7"));
    writer.formatElement("rdb_hdr_user8",  "%d",   processingControl->Value<int>("UserValue8"));
    writer.formatElement("rdb_hdr_user9",  "%d",   processingControl->Value<int>("UserValue9"));
    writer.formatElement("rdb_hdr_user10", "%d",   processingControl->Value<int>("UserValue10"));
    writer.formatElement("rdb_hdr_user11", "%d",   processingControl->Value<int>("UserValue11"));
    writer.formatElement("rdb_hdr_user12", "%d",   processingControl->Value<int>("UserValue12"));
    writer.formatElement("rdb_hdr_user13", "%d",   processingControl->Value<int>("UserValue13"));
    writer.formatElement("rdb_hdr_user14", "%d",   processingControl->Value<int>("UserValue14"));
    writer.formatElement("rdb_hdr_user15", "%d",   processingControl->Value<int>("UserValue15"));
    writer.formatElement("rdb_hdr_user16", "%d",   processingControl->Value<int>("UserValue16"));
    writer.formatElement("rdb_hdr_user17", "%d",   processingControl->Value<int>("UserValue17"));
    writer.formatElement("rdb_hdr_user18", "%d",   processingControl->Value<int>("UserValue18"));
    writer.formatElement("rdb_hdr_user19", "%d",   processingControl->Value<int>("UserValue19"));
    writer.endElement();

    if (lxData->IsEpi())
    {
        // Create EPI processing control object, so all relevant varaibles within that object are
        // avaliable and accessible.

        boost::shared_ptr<GERecon::Epi::LxControlSource> const controlSource = boost::make_shared<GERecon::Epi::LxControlSource>(lxData);
        GERecon::Control::ProcessingControlPointer               procCtrlEPI = controlSource->CreateOrchestraProcessingControl();
        GERecon::Acquisition::ArchiveStoragePointer      archive_storage_ptr = GERecon::Acquisition::ArchiveStorage::Create(scanArchive_);

        int ref_views                                        = procCtrlEPI->Value<int>("ExtraFramesTop") + procCtrlEPI->Value<int>("ExtraFramesBottom");

        // In EPI ScanArchive files, the number of acquisitions == (number of slices per volume + 1 (control packet)) * number of volumes.
        //
        // So to recover number of volumes / repetitions, just invert this relationship.  This may be fragile, if other types of packets,
        // with different OpCodes - start getting included in the ScanArvhive.
        int num_volumes                                      = archive_storage_ptr->AvailableControlCount() /
                                                               (processingControl->Value<int>("NumSlices") + 1);

        writer.startElement("epiParameters");
          writer.addBooleanElement("isEpiRefScanIntegrated",   procCtrlEPI->Value<bool>("IntegratedReferenceScan"));
          writer.addBooleanElement("MultibandEnabled",         procCtrlEPI->ValueStrict<bool>("MultibandEnabled"));
          writer.formatElement("ExtraFramesTop", "%d",         procCtrlEPI->Value<int>("ExtraFramesTop"));
          writer.formatElement("AcquiredYRes", "%d",           procCtrlEPI->Value<int>("AcquiredYRes"));
          writer.formatElement("ExtraFramesBottom", "%d",      procCtrlEPI->Value<int>("ExtraFramesBottom"));
          // writer.formatElement("NumRefViews", "%d",            procCtrlEPI->Value<int>("NumRefViews")); // not found at run time up to Orchestra 1.10.1
          writer.formatElement("NumRefViews", "%d",            ref_views);
          writer.formatElement("num_volumes", "%d",            num_volumes);
          // writer.formatElement("nMultiBandSlices", "%d",    procCtrlEPI->ValueStrict<int>("MultibandNumAcquiredSlices"));
          // writer.formatElement("NumberOfShots", "%d",       procCtrlEPI->Value<unsigned int>("NumberOfShots"));
          // writer.formatElement("NumAcqsPerRep", "%d",       procCtrlEPI->Value<int>("NumAcquisitionsPerRepetition"));
          // writer.formatElement("DataSampleTime", "%f",      processingControl->Value<float>("A2DSampleTime")); // in usec. only valid if IsSpiral = true
       writer.endElement();
    }

    writer.endDocument();

    // DEBUG: std::cerr << "XML stream from GE is: " << writer.getXML().c_str() << std::endl;

    return writer.getXML();
}

} // namespace GEToIsmrmrd

