#pragma once
#ifndef PLUGIN_H
#define PLUGIN_H

#include "SequenceConverter.h"

namespace PfileToIsmrmrd {

class Plugin
{
public:
    Plugin(const std::string& filepath, const std::string& classname)
    {
        load(filepath, classname);
    }
    ~Plugin() {}
    std::shared_ptr<SequenceConverter> getConverter();

private:
    // non-copyable
    Plugin(const Plugin& other);
    Plugin& operator=(const Plugin& other);

    void load(const std::string& filepath, const std::string& classname);

    std::shared_ptr<void> dll_;
    std::shared_ptr<SequenceConverter> converter_;
};

} // namespace PfileToIsmrmrd

#endif // PLUGIN_H
