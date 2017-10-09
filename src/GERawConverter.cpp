
/** @file GERawConverter.cpp */
#include <iostream>
#include <stdexcept>

#include <libxml/xmlschemas.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <Orchestra/Common/ArchiveHeader.h>
#include <Orchestra/Common/PrepData.h>

#include <Orchestra/Common/ImageCorners.h>

#include <Orchestra/Control/ProcessingControl.h>
#include <Orchestra/Legacy/Pfile.h>
#include <Orchestra/Legacy/DicomSeries.h>
#include <Dicom/MR/Image.h>
#include <Dicom/MR/ImageModule.h>
#include <Dicom/MR/PrivateAcquisitionModule.h>
#include <Dicom/Patient.h>
#include <Dicom/PatientModule.h>
#include <Dicom/PatientStudyModule.h>
#include <Dicom/Equipment.h>
#include <Dicom/EquipmentModule.h>
#include <Dicom/ImagePlaneModule.h>

// Local
#include "GERawConverter.h"
#include "XMLWriter.h"
#include "ge_tools_path.h"


namespace PfileToIsmrmrd {

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

static std::string pfile_to_xml(const GERecon::Legacy::PfilePointer pfile);

/**
 * Creates a GERawConverter from an ifstream of the PFile header
 *
 * @param fp raw FILE pointer to PFile
 * @throws std::runtime_error if P-File cannot be read
 */
GERawConverter::GERawConverter(const std::string& pfilepath, bool logging)
    : log_(logging)
{
    FILE* fp = NULL;
    if (!(fp = fopen(pfilepath.c_str(), "rb"))) {
        throw std::runtime_error("Failed to open " + pfilepath);
    }

    psdname_ = ""; // TODO: find PSD Name in Orchestra Pfile class
    log_ << "PSDName: " << psdname_ << std::endl;

    // Using Orchestra
    pfile_ = GERecon::Legacy::Pfile::Create(pfilepath,
            GERecon::Legacy::Pfile::AllAvailableAcquisitions,
            GERecon::AnonymizationPolicy(GERecon::AnonymizationPolicy::None));
}

/**
 * Creates a GERawConverter from a pointer to the PFile header
 *
 * @param hdr_loc pointer to PFile header in memory
 */
/*
GERawConverter::GERawConverter(void *hdr_loc, bool logging)
    : log_(logging)
{
    pfile_ = std::shared_ptr<pfile_t> (pfile_load_from_raw(hdr_loc, NULL, 0), pfile_destroy);
    if (!pfile_) {
        throw std::runtime_error("Failed to create P-File instance");
    }
    psdname_ = std::string(pfile_->psd_name);
    log_ << "PSDName: " << psdname_ << std::endl;
}
*/

/**
 * Loads a sequence conversion plugin from full filepath
 */
void GERawConverter::usePlugin(const std::string& filename, const std::string& classname)
{
    log_ << "Loading plugin: " << filename << ":" << classname << std::endl;
    plugin_ = std::shared_ptr<Plugin>(new Plugin(filename, classname));
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

void GERawConverter::useConfigFilename(const std::string& filename)
{
    log_ << "Loading configuration: " << filename << std::endl;
    std::ifstream stream(filename.c_str(), std::ios::binary);
    useConfigStream(stream);
}

void GERawConverter::useConfigStream(std::ifstream& stream)
{
    stream.seekg(0, std::ios::beg);

    std::string config((std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>());
    useConfigString(config);
}

bool GERawConverter::validateConfig(std::shared_ptr<xmlDoc> config_doc)
{
    log_ << "Validating configuration" << std::endl;

    std::shared_ptr<xmlDoc> schema_doc = std::shared_ptr<xmlDoc>(
            xmlParseMemory(g_schema.c_str(), g_schema.size()), xmlFreeDoc);
    if (!schema_doc) {
        throw std::runtime_error("Failed to parse embedded config-file schema");
    }

    std::shared_ptr<xmlSchemaParserCtxt> parser_ctx = std::shared_ptr<xmlSchemaParserCtxt>(
            xmlSchemaNewDocParserCtxt(schema_doc.get()), xmlSchemaFreeParserCtxt);
    if (!parser_ctx) {
        throw std::runtime_error("Failed to create schema parser");
    }

    std::shared_ptr<xmlSchema> schema = std::shared_ptr<xmlSchema>(
            xmlSchemaParse(parser_ctx.get()), xmlSchemaFree);
    if (!schema) {
        throw std::runtime_error("Failed to create schema");
    }

    std::shared_ptr<xmlSchemaValidCtxt> valid_ctx = std::shared_ptr<xmlSchemaValidCtxt>(
            xmlSchemaNewValidCtxt(schema.get()), xmlSchemaFreeValidCtxt);
    if (!valid_ctx) {
        throw std::runtime_error("Failed to create schema validity context");
    }

    // Set error/warning logging functions
    // xmlSchemaSetValidErrors(valid_ctx, errors, warnings, NULL);

    if (xmlSchemaValidateDoc(valid_ctx.get(), config_doc.get()) == 0) {
        return true;
    }
    return false;
}

/**
 * Validates configuration then loads plugin, stylesheet
 *
 * TODO: Leaks memory if exception thrown
 */
void GERawConverter::useConfigString(const std::string& config)
{
    std::string error_message;

    std::shared_ptr<xmlDoc> config_doc = std::shared_ptr<xmlDoc>(
            xmlParseMemory(config.c_str(), config.size()), xmlFreeDoc);
    if (!config_doc) {
        throw std::runtime_error("Failed to parse config");
    }

    if (!validateConfig(config_doc)) {
        throw std::runtime_error("Invalid configuration");
    }

    log_ << "Searching for sequence mapping" << std::endl;

    xmlNodePtr cur = xmlDocGetRootElement(config_doc.get());
    if (NULL == cur) {
        throw std::runtime_error("Can't get root element of configuration");
    }

    if (xmlStrcmp(cur->name, (const xmlChar *)"conversionConfiguration")) {
        throw std::runtime_error("root element should be \"conversionConfiguration\"");
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
        if (xmlStrcmp(cur->name, (const xmlChar*) "sequenceMapping") == 0) {
            if (trySequenceMapping(config_doc, cur)) {
                break;
            }
        }

        cur = cur->next;
    }
}

/**
 * Attempts to load and use a sequence mapping from an XML config.
 *
 * Returns `true` on success, `false` otherwise.
 */
bool GERawConverter::trySequenceMapping(std::shared_ptr<xmlDoc> doc, xmlNodePtr mapping)
{
    xmlNodePtr parameter = mapping->xmlChildrenNode;
    std::string psdname, libpath, classname, stylesheet, reconconfig;

    while (parameter != NULL) {
        if (xmlStrcmp(parameter->name, (const xmlChar*)"psdname") == 0) {
            char *tmp = (char*)xmlNodeListGetString(doc.get(), parameter->xmlChildrenNode, 1);
            psdname = std::string(tmp);
            xmlFree(tmp);
        } else if (xmlStrcmp(parameter->name, (const xmlChar*)"libraryPath") == 0) {
            char *tmp = (char*)xmlNodeListGetString(doc.get(), parameter->xmlChildrenNode, 1);
            libpath = std::string(tmp);
            xmlFree(tmp);
        } else if (xmlStrcmp(parameter->name, (const xmlChar*)"className") == 0) {
            char *tmp = (char*)xmlNodeListGetString(doc.get(), parameter->xmlChildrenNode, 1);
            classname = std::string(tmp);
            xmlFree(tmp);
        } else if (xmlStrcmp(parameter->name, (const xmlChar*)"stylesheet") == 0) {
            char *tmp = (char*)xmlNodeListGetString(doc.get(), parameter->xmlChildrenNode, 1);
            stylesheet = std::string(tmp);
            xmlFree(tmp);
        } else if (xmlStrcmp(parameter->name, (const xmlChar*)"reconConfigName") == 0) {
            char *tmp = (char*)xmlNodeListGetString(doc.get(), parameter->xmlChildrenNode, 1);
            reconconfig = std::string(tmp);
            xmlFree(tmp);
        }
        parameter = parameter->next;
    }

    bool success = false;
    /* check if psdname matches */
    if (xmlStrcmp(BAD_CAST psdname.c_str(), (const xmlChar*) psdname_.c_str()) == 0) {
        log_ << "Found matching psdname: " << psdname << std::endl;

        std::string ge_tools_home(get_ge_tools_home());
#ifdef __APPLE__
        std::string ext = ".dylib";
#else
        std::string ext = ".so";
#endif
        libpath = ge_tools_home + "lib/" + libpath + ext;
        stylesheet = ge_tools_home + "share/ge-tools/config/" + stylesheet;

        usePlugin(libpath, classname);
        useStylesheetFilename(stylesheet);
        recon_config_ = std::string(reconconfig);
        success = true;
    }

    return success;
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

    std::string pfile_header(pfile_to_xml(pfile_));

    // DEBUG: std::cout << pfile_header << std::endl;

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
            xmlParseMemory(pfile_header.c_str(), pfile_header.size()), xmlFreeDoc);
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
 * Gets the acquisitions corresponding to a view in memory (P-file).
 *
 * @param view_num View number to get
 * @param vacq Vector of acquisitions
 * @throws std::runtime_error { if plugin fails to copy the data }
 */
std::vector<ISMRMRD::Acquisition> GERawConverter::getAcquisitions(unsigned int view_num)
{
    return plugin_->getConverter()->getAcquisitions(pfile_.get(), view_num);
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

/**
 * Gets the number of views in the pfile
 */
unsigned int GERawConverter::getNumViews(void)
{
  return pfile_->ViewCount();
}

/**
 * Sets the PFile origin to the RDS client
 *
 * TODO: implement!
 */
void GERawConverter::setRDS(void)
{
}

static std::string pfile_to_xml(const GERecon::Legacy::PfilePointer pfile)
{
    XMLWriter writer;

    writer.startDocument();

    writer.startElement("Header");

    writer.formatElement("RunNumber", "%d", pfile->RunNumber());
    writer.formatElement("AcqCount", "%d", pfile->AcqCount());
    writer.formatElement("PassCount", "%d", pfile->PassCount());
    writer.formatElement("PfileCount", "%d", pfile->PfileCount());
    writer.addBooleanElement("IsConcatenated", pfile->IsConcatenated());
    writer.addBooleanElement("IsRawMode", pfile->IsRawMode());
    writer.formatElement("SliceCount", "%d", pfile->SliceCount());
    writer.formatElement("SlicesPerAcq", "%d", pfile->AcquiredSlicesPerAcq());
    writer.formatElement("EchoCount", "%d", pfile->EchoCount());
    writer.formatElement("ChannelCount", "%d", pfile->ChannelCount());
    writer.formatElement("PhaseCount", "%d", pfile->PhaseCount());
    writer.formatElement("XRes", "%d", pfile->XRes());
    writer.formatElement("YRes", "%d", pfile->YRes());
    writer.formatElement("RawDataSize", "%llu", pfile->RawDataSize());
    writer.formatElement("TotalChannelSize", "%llu", pfile->TotalChannelSize());
    writer.formatElement("ViewCount", "%u", pfile->ViewCount());
    writer.formatElement("ViewSize", "%u", pfile->ViewSize());
    writer.formatElement("SampleSize", "%u", pfile->SampleSize());
    //writer.formatElement("SampleType", "%d", pfile->SampleType());
    writer.formatElement("BaselineViewCount", "%d", pfile->BaselineViewCount());
    writer.addBooleanElement("Is3D", pfile->Is3D());
    // writer.addBooleanElement("Is3DIfftDone", pfile->Is3DIfftDone());
    writer.addBooleanElement("IsRadial3D", pfile->IsRadial3D());
    writer.formatElement("PlaneCount", "%d", pfile->PlaneCount());
    writer.formatElement("OutputPhaseCount", "%d", pfile->OutputPhaseCount());
    writer.formatElement("ShotCount", "%d", pfile->ShotCount());
    writer.formatElement("RepetitionCount", "%d", pfile->RepetitionCount());
    writer.addBooleanElement("IsEpi", pfile->IsEpi());
    writer.addBooleanElement("IsPerChannelMultiVoxelSpectro", pfile->IsPerChannelMultiVoxelSpectro());
    writer.addBooleanElement("IsMCSI", pfile->IsMCSI());
    writer.addBooleanElement("IsSingleVoxel", pfile->IsSingleVoxel());
    writer.addBooleanElement("IsDiffusionEpi", pfile->IsDiffusionEpi());
    writer.addBooleanElement("IsMultiPhaseEpi", pfile->IsMultiPhaseEpi());
    writer.addBooleanElement("IsFunctionalMri", pfile->IsFunctionalMri());
    writer.addBooleanElement("IsTricks", pfile->IsTricks());
    writer.addBooleanElement("IsCine", pfile->IsCine());
    writer.addBooleanElement("IsCalibration", pfile->IsCalibration());
    writer.addBooleanElement("IsMavric", pfile->IsMavric());
    //writer.formatElement("NumEpiDiffusionNexes", "%d", pfile->NumEpiDiffusionNexes());
    writer.addBooleanElement("IsTopDownEpi", pfile->IsTopDownEpi());
    writer.addBooleanElement("IsBottomUpEpi", pfile->IsBottomUpEpi());
    //writer.formatElement("NumberOfNexs", "%d", pfile->NumberOfNexs());
    writer.formatElement("MultiPhaseType", "%d", pfile->MultiPhaseType()); // this is interleaved field

    const GERecon::Legacy::LxDownloadDataPointer lxData = pfile->DownloadData();

    GERecon::Legacy::DicomSeries legacySeries(lxData);
    GEDicom::SeriesPointer series = legacySeries.Series();
    GEDicom::SeriesModulePointer seriesModule = series->GeneralModule();
    writer.startElement("Series");
    writer.formatElement("Number", "%d", lxData->SeriesNumber());
    writer.formatElement("UID", "%s", seriesModule->UID().c_str());
    writer.formatElement("Description", "%s", seriesModule->SeriesDescription().c_str());
    //writer.formatElement("Modality", "%s", seriesModule->Modality());
    writer.formatElement("Laterality", "%s", seriesModule->Laterality().c_str());
    writer.formatElement("Date", "%s", seriesModule->Date().c_str());
    writer.formatElement("Time", "%s", seriesModule->Time().c_str());
    writer.formatElement("ProtocolName", "%s", seriesModule->ProtocolName().c_str());
    writer.formatElement("OperatorName", "%s", seriesModule->OperatorName().c_str());
    writer.formatElement("PpsDescription", "%s", seriesModule->PpsDescription().c_str());
    //writer.formatElement("PatientEntry", "%s", seriesModule->Entry());
    //writer.formatElement("PatientOrientation", "%s", seriesModule->Orientation());
    writer.endElement();

    GEDicom::StudyPointer study = series->Study();
    GEDicom::StudyModulePointer studyModule = study->GeneralModule();
    writer.startElement("Study");
    writer.formatElement("Number", "%d", studyModule->StudyNumber());
    writer.formatElement("UID", "%s", studyModule->UID().c_str());
    writer.formatElement("Description", "%s", studyModule->StudyDescription().c_str());
    writer.formatElement("Date", "%s", studyModule->Date().c_str());
    writer.formatElement("Time", "%s", studyModule->Time().c_str());
    writer.formatElement("ReferringPhysician", "%s", studyModule->ReferringPhysician().c_str());
    writer.formatElement("AccessionNumber", "%s", studyModule->AccessionNumber().c_str());
    writer.formatElement("ReadingPhysician", "%s", studyModule->ReadingPhysician().c_str());
    writer.endElement();

    GEDicom::PatientStudyModulePointer patientStudyModule = study->PatientStudyModule();
    GEDicom::PatientPointer patient = study->Patient();
    GEDicom::PatientModulePointer patientModule = patient->GeneralModule();
    writer.startElement("Patient");
    writer.formatElement("Name", "%s", patientModule->Name().c_str());
    writer.formatElement("ID", "%s", patientModule->ID().c_str());
    writer.formatElement("Birthdate", "%s", patientModule->Birthdate().c_str());
    writer.formatElement("Gender", "%s", patientModule->Gender().c_str());
    writer.formatElement("Age", "%s", patientStudyModule->Age().c_str());
    writer.formatElement("Weight", "%s", patientStudyModule->Weight().c_str());
    writer.formatElement("History", "%s", patientStudyModule->History().c_str());
    writer.endElement();

    GEDicom::EquipmentPointer equipment = series->Equipment();
    GEDicom::EquipmentModulePointer equipmentModule = equipment->GeneralModule();
    writer.startElement("Equipment");
    writer.formatElement("Manufacturer", "%s", equipmentModule->Manufacturer().c_str());
    writer.formatElement("Institution", "%s", equipmentModule->Institution().c_str());
    writer.formatElement("Station", "%s", equipmentModule->Station().c_str());
    writer.formatElement("ManufacturerModel", "%s", equipmentModule->ManufacturerModel().c_str());
    writer.formatElement("DeviceSerialNumber", "%s", equipmentModule->DeviceSerialNumber().c_str());
    writer.formatElement("SoftwareVersion", "%s", equipmentModule->SoftwareVersion().c_str());
    writer.formatElement("PpsPerformedStation", "%s", equipmentModule->PpsPerformedStation().c_str());
    writer.formatElement("PpsPerformedLocation", "%s", equipmentModule->PpsPerformedLocation().c_str());
    writer.endElement();


    const GERecon::Control::ProcessingControlPointer processingControl(pfile->CreateOrchestraProcessingControl());

    writer.formatElement("AcquiredXRes", "%d", processingControl->Value<int>("AcquiredXRes"));
    writer.formatElement("AcquiredYRes", "%d", processingControl->Value<int>("AcquiredYRes"));
    writer.formatElement("AcquiredZRes", "%d", processingControl->Value<int>("AcquiredZRes"));
    writer.addBooleanElement("Is3DAcquisition", processingControl->Value<bool>("Is3DAcquisition"));

    writer.formatElement("NumBaselineViews", "%d", processingControl->Value<int>("NumBaselineViews"));
    writer.formatElement("NumVolumes", "%d", processingControl->Value<int>("NumVolumes"));
    writer.formatElement("NumEchoes", "%d", processingControl->Value<int>("NumEchoes"));
    writer.addBooleanElement("EvenEchoFrequencyFlip", processingControl->Value<bool>("EvenEchoFrequencyFlip"));
    writer.addBooleanElement("OddEchoFrequencyFlip", processingControl->Value<bool>("OddEchoFrequencyFlip"));
    writer.addBooleanElement("EvenEchoPhaseFlip", processingControl->Value<bool>("EvenEchoPhaseFlip"));
    writer.addBooleanElement("OddEchoPhaseFlip", processingControl->Value<bool>("OddEchoPhaseFlip"));
    writer.addBooleanElement("HalfEcho", processingControl->Value<bool>("HalfEcho"));
    writer.addBooleanElement("HalfNex", processingControl->Value<bool>("HalfNex"));
    writer.addBooleanElement("NoFrequencyWrapData", processingControl->Value<bool>("NoFrequencyWrapData"));
    writer.addBooleanElement("NoPhaseWrapData", processingControl->Value<bool>("NoPhaseWrapData"));
    writer.formatElement("NumAcquisitions", "%d", processingControl->Value<int>("NumAcquisitions"));
    writer.formatElement("NumEchoes", "%d", processingControl->Value<int>("NumEchoes"));
    writer.formatElement("DataSampleSize", "%d", processingControl->Value<int>("DataSampleSize")); // in bytes
                                                                                                   // nacq_points = ncoils * frame_size

    std::string patientPosition = GERecon::PatientPositionAsString(static_cast<GERecon::PatientPosition>(processingControl->Value<int>("PatientPosition")));
    writer.formatElement("PatientPosition", "%s", patientPosition.c_str());
    writer.formatElement("PatientEntry", "%d", processingControl->Value<int>("PatientEntry")); // no PatientEntryAsString function in Orchestra

    writer.formatElement("ScanCenter", "%f", processingControl->Value<int>("ScanCenter"));
    writer.formatElement("Landmark", "%f", processingControl->Value<int>("Landmark"));
    writer.formatElement("ExamNumber", "%u", processingControl->Value<int>("ExamNumber"));
    writer.formatElement("CoilConfigUID", "%u", processingControl->Value<int>("CoilConfigUID"));
    writer.formatElement("RawPassSize", "%llu", processingControl->Value<int>("RawPassSize"));

    // see AcquisitionParameters documentation for more boolean parameters

    // ReconstructionParameters
    writer.addBooleanElement("CreateMagnitudeImages", processingControl->Value<bool>("CreateMagnitudeImages"));
    writer.addBooleanElement("CreatePhaseImages", processingControl->Value<bool>("CreatePhaseImages"));

    writer.formatElement("TransformXRes", "%d", processingControl->Value<int>("TransformXRes"));
    writer.formatElement("TransformYRes", "%d", processingControl->Value<int>("TransformYRes"));
    writer.formatElement("TransformZRes", "%d", processingControl->Value<int>("TransformZRes"));

    writer.addBooleanElement("ChopX", processingControl->Value<bool>("ChopX"));
    writer.addBooleanElement("ChopY", processingControl->Value<bool>("ChopY"));
    writer.addBooleanElement("ChopZ", processingControl->Value<bool>("ChopZ"));

    // TODO: map SliceOrder to a string
    // writer.formatElement("SliceOrder", "%s", processingControl->Value<int>("SliceOrder"));

    // Image Parameters
    // writer.formatElement("ImageXRes", "%d", processingControl->Value<int>("ImageXRes"));
    // writer.formatElement("ImageYRes", "%d", processingControl->Value<int>("ImageYRes"));


    GERecon::PrepData prepData(lxData);
    GERecon::ArchiveHeader archiveHeader("ScanArchive", prepData);
    // DEBUG: archiveHeader.Print(std::cout);

    auto sliceOrientation = pfile->Orientation(0);
    auto sliceCorners = pfile->Corners(0);
    auto imageCorners = GERecon::ImageCorners(sliceCorners, sliceOrientation);
    auto grayscaleImage = GEDicom::GrayscaleImage(128, 128);
    auto dicomImage = GERecon::Legacy::DicomImage(grayscaleImage, 0, imageCorners, series, *lxData);
    auto imageModule = dicomImage.ImageModule();

    writer.startElement("Image");
    writer.formatElement("EchoTime", "%s", imageModule->EchoTime().c_str());
    writer.formatElement("RepetitionTime", "%s", imageModule->RepetitionTime().c_str());
    writer.formatElement("InversionTime", "%s", imageModule->InversionTime().c_str());
    writer.formatElement("ImageType", "%s", imageModule->ImageType().c_str());
    writer.formatElement("ScanSequence", "%s", imageModule->ScanSequence().c_str());
    writer.formatElement("SequenceVariant", "%s", imageModule->SequenceVariant().c_str());
    writer.formatElement("ScanOptions", "%s", imageModule->ScanOptions().c_str());
    writer.formatElement("AcquisitionType", "%d", imageModule->AcqType());
    writer.formatElement("PhaseEncodeDirection", "%d", imageModule->PhaseEncodeDirection());
    writer.formatElement("ImagingFrequency", "%s", imageModule->ImagingFrequency().c_str());
    writer.formatElement("MagneticFieldStrength", "%s", imageModule->MagneticFieldStrength().c_str());
    writer.formatElement("SliceSpacing", "%s", imageModule->SliceSpacing().c_str());
    writer.formatElement("FlipAngle", "%s", imageModule->FlipAngle().c_str());
    writer.formatElement("EchoTrainLength", "%s", imageModule->EchoTrainLength().c_str());

    auto imageModuleBase = dicomImage.ImageModuleBase();
    writer.formatElement("AcquisitionDate", "%s", imageModuleBase->AcquisitionDate().c_str());
    writer.formatElement("AcquisitionTime", "%s", imageModuleBase->AcquisitionTime().c_str());
    writer.formatElement("ImageDate", "%s", imageModuleBase->ImageDate().c_str());
    writer.formatElement("ImageTime", "%s", imageModuleBase->ImageTime().c_str());

    auto imagePlaneModule = dicomImage.ImagePlaneModule();
    writer.formatElement("ImageOrientation", "%s", imagePlaneModule->ImageOrientation().c_str());
    writer.formatElement("ImagePosition", "%s", imagePlaneModule->ImagePosition().c_str());
    writer.formatElement("SliceThickness", "%f", imagePlaneModule->SliceThickness());
    writer.formatElement("SliceLocation", "%f", imagePlaneModule->SliceLocation());
    writer.formatElement("PixelSizeX", "%f", imagePlaneModule->PixelSizeX());
    writer.formatElement("PixelSizeY", "%f", imagePlaneModule->PixelSizeY());

    auto privateAcquisitionModule = dicomImage.PrivateAcquisitionModule();
    writer.formatElement("SecondEcho", "%s", privateAcquisitionModule->SecondEcho().c_str());

    writer.endElement();

    // Old fields from headers:

    // TODO: scan_type field

    // TODO: data_collect_type field
    // TODO: data_collect_type1 field
    // TODO: data_format field
    // TODO: dacq_ctrl field
    // TODO: recon_type field

    // TODO: patient_pos field

    // TODO: psd_name field

    // TODO: dfov field
    // TODO: dfov_rect field

    // TODO: te2 field
    // TODO: rdb_te field
    // TODO: rdb_te2 field
    // TODO: ti field
    // std::cout << "InversionTime is " << imageModule->InversionTime().c_str() << std::endl;
    // TODO: trigger_time field

    // TODO: measurement_uid field
    // TODO: variable_bandwidth field
    // TODO: sample_time field

    // TODO: study_iuid field
    // TODO: series_iuid field
    // TODO: frame_of_reference_iuid field

    // TODO: referenced_image_iuids

    // TODO: rdb_hdr_user fields

    writer.endDocument();

    return writer.getXML();
}

} // namespace PfileToIsmrmrd

