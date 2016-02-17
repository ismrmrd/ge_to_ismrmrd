#include "Plugin.h"

#include <dlfcn.h>
#include <stdexcept>

namespace PfileToIsmrmrd {

/**
 * Loads the given plugin from the specified library, including its
 * construction and destruction functions.
 *
 * @param filepath full file path to the dynamic library containing the plugin
 * @param classname the full name of the class comprising the conversion plugin
 * @throws std::runtime_error
 */
void Plugin::load(const std::string& filepath, const std::string& classname)
{
    const char *dlsym_error = dlerror();        // Reset error indicator

    // Attempt to open the library
    dll_ = std::shared_ptr<void>(dlopen(filepath.c_str(), RTLD_LAZY), dlclose);
    dlsym_error = dlerror();
    if (!dll_ || dlsym_error) {
        throw std::runtime_error("Failed to open library " + filepath +
                " (" + std::string(dlsym_error) + ")");
    }

    // Construct the name of the factory function (make_PluginName)
    std::string maker("make_" + classname);
    // then try to open it
    typedef SequenceConverter* (*maker_type)();
    maker_type create_converter = (maker_type) dlsym(dll_.get(), maker.c_str());
    dlsym_error = dlerror();
    if (dlsym_error) {
        throw std::runtime_error("Failed to load plugin function " + maker +
                " ( " + std::string(dlsym_error) + ")");
    }

    // Construct the name of the plugin destruction function (destroy_PluginName)
    std::string destroyer = "destroy_" + classname;
    // then try to open it
    typedef void (*destroyer_type)(SequenceConverter *);
    destroyer_type destroy_converter = (destroyer_type) dlsym(dll_.get(), destroyer.c_str());
    dlsym_error = dlerror();
    if (dlsym_error) {
        throw std::runtime_error("Failed to load plugin function " + destroyer +
                " (" + std::string(dlsym_error) + ")");
    }

    // Make `new` SequenceConverter class from DLL
    try {
        converter_ = std::shared_ptr<SequenceConverter>(create_converter(), destroy_converter);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create converter (" + std::string(e.what()) + ")");
    }
}

std::shared_ptr<SequenceConverter> Plugin::getConverter()
{
    // Call the factory function, allocating a Sequence Converter object
    return converter_;
}

} // namespace PfileToIsmrmrd
