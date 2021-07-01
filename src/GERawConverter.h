/** @file GERawConverter.h */
#ifndef GE_RAW_CONVERTER_H
#define GE_RAW_CONVERTER_H

#include <fstream>

// ISMRMRD
#include "ismrmrd/ismrmrd.h"

// Local
#include "Plugin.h"
#include "SequenceConverter.h"
#include "GenericConverter.h"
#include "NIHPlugins/epiConverter.h"

// Libxml2 forward declarations
struct _xmlDoc;
struct _xmlNode;

namespace PfileToIsmrmrd {

struct logstream {
    logstream(bool enable) : enabled(enable) {}
    bool enabled;
};

template <typename T>
logstream& operator<<(logstream& s, T const& v)
{
    if (s.enabled) { std::clog << v; }
    return s;
}

logstream& operator<<(logstream& s, std::ostream& (*f)(std::ostream&))
{
    if (s.enabled) { f(std::clog); }
    return s;
}

enum GE_RAW_TYPES
{
   SCAN_ARCHIVE_RAW_TYPE = 0,
   PFILE_RAW_TYPE = 1,
   MISC_RAW_TYPE = 99
};


class GERawConverter
{
public:
    GERawConverter(const std::string& pfilepath, bool logging=false);

    void usePlugin(const std::string& filename, const std::string& classname);
    std::shared_ptr<SequenceConverter> useConverter(const std::string& classname);
    std::shared_ptr<SequenceConverter> getConverter();

    void useStylesheetFilename(const std::string& filename);
    void useStylesheetStream(std::ifstream& stream);
    void useStylesheetString(const std::string& sheet);

    void useConfigFilename(const std::string& filename);
    void useConfigStream(std::ifstream& stream);
    void useConfigString(const std::string& config);

    std::string getIsmrmrdXMLHeader();

    std::vector<ISMRMRD::Acquisition> getAcquisitions(unsigned int view_num);

    std::string getReconConfigName(void);

private:
    // Non-copyable
    GERawConverter(const GERawConverter& other);
    GERawConverter& operator=(const GERawConverter& other);

    bool validateConfig(std::shared_ptr<struct _xmlDoc> config_doc);
    bool trySequenceMapping(std::shared_ptr<struct _xmlDoc> doc, struct _xmlNode* mapping);

    std::string psdname_;
    std::string recon_config_;
    std::string stylesheet_;

    GERecon::Legacy::PfilePointer pfile_;

    GERecon::ScanArchivePointer scanArchive_;
    GERecon::Legacy::LxDownloadDataPointer lxData_;
    GERecon::Control::ProcessingControlPointer processingControl_;
    int rawObjectType_; // to allow reference to a P-File or ScanArchive object
    std::shared_ptr<Plugin> plugin_;
    std::shared_ptr<PfileToIsmrmrd::SequenceConverter> converter_;

    logstream log_;
};

} // namespace PfileToIsmrmrd

#endif  // GE_RAW_CONVERTER_H
