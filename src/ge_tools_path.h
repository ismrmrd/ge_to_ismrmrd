#ifndef GE_TOOLS_PATHS_H
#define GE_TOOLS_PATHS_H

#include <limits.h>
#include <string>
#include <iostream>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#endif

#define MAX_GE_TOOLS_HOME_LENGTH 1024

inline std::string get_ge_tools_home()
{
#if defined  __APPLE__
    char path[MAX_GE_TOOLS_HOME_LENGTH];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string s1(path);
        return s1.substr(0, s1.find_last_of("\\/")) + std::string("/../");
    } else {
        std::cout << "Unable to determine GE_TOOLS_HOME" << std::endl;
        return std::string("");
    }
#else // Probably some NIX where readlink should work
    char buff[MAX_GE_TOOLS_HOME_LENGTH];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
    if (len != -1) {
        buff[len] = '\0';
        std::string s1(buff);
        return s1.substr(0, s1.find_last_of("\\/")) + std::string("/../");
    } else {
        std::cout << "Unable to determine GE_TOOLS_HOME" << std::endl;
        return std::string("");
    }
#endif
}

#endif // GE_TOOLS_PATHS_H
