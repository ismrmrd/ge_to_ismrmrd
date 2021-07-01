
#include <cstdio>

// Boost
#include <boost/program_options.hpp>

// ISMRMRD
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"


// GE
#include "GERawConverter.h"
#include "ge_tools_path.h"

namespace po = boost::program_options;

int main (int argc, char *argv[])
{
    std::string configfile, libpath, classname, stylesheet, rawFile, outfile;

    std::string thisProgram = argv[0];
    std::string validInputs = "input P- or ScanArchive File";
    std::string usage = thisProgram + " [options] <" + validInputs + ">";
    std::string config_default = get_ge_tools_home() + "share/ge-tools/config/default.xml";
    std::string stylesheet_default = get_ge_tools_home() + "share/ge-tools/config/default.xsl";
    std::string library_file_default = get_ge_tools_home() + "lib/libg2i-generic.so";
    std::string sequence_class_default = "GenericConverter";

    po::options_description basic("Basic Options");
    basic.add_options()
        ("help,h", "print help message")
        ("verbose,v", "enable verbose mode")
        ("library,l", po::value<std::string>(&libpath)->default_value(library_file_default), "name of library with converter plugin, or full path if not in $LD_LIBRARY_PATH")
        ("config,c", po::value<std::string>(&configfile)->default_value(config_default), "XML configuration file mapping acquisition sequence to converter class")
        ("plugin,p", po::value<std::string>(&classname)->default_value(sequence_class_default), "class/sequence name in library used for conversion")
        ("stylesheet,x", po::value<std::string>(&stylesheet)->default_value(stylesheet_default), "XSL stylesheet file mapping values provided by Orchestra to those needed by ISMRMRD")
        ("output,o", po::value<std::string>(&outfile)->default_value("testdata.h5"), "output HDF5 file")
        ("string,s", "only print the HDF5 XML header")
        ;

    po::options_description input("Input Options");
    input.add_options()
        ("input,i", po::value<std::string>(&rawFile), validInputs.c_str())
        ;

    po::options_description all_options("Options");
    all_options.add(basic).add(input);

    po::options_description visible_options("Options");
    visible_options.add(basic);

    po::positional_options_description positionals;
    positionals.add("input", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(positionals).run(), vm);

        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << usage << std::endl << visible_options << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.count("help")) {
        std::cerr << usage << std::endl << visible_options << std::endl;

         // Print System options (if available).  Code to iterate through
         // a boost::program_options::variable_map was taken from:
         //
         //    https://stackoverflow.com/questions/21008893/boost-program-options-iterate-over-variables-map

         // for (const auto& it : commandLineOptions.AllOptions()) {
            // std::cout << it.first.c_str() << " ";
            // auto& value = it.second.value();
            // if (auto v = boost::any_cast<uint32_t>(&value))
               // std::cout << *v;
            // else if (auto v = boost::any_cast<std::string>(&value))
               // std::cout << *v;
            // else
               // std::cout << "error";
         // }

        return EXIT_SUCCESS;
    }

    if (rawFile.size() == 0) {
        std::cerr << usage << std::endl;
        return EXIT_FAILURE;
    }

    bool verbose = false;
    if (vm.count("verbose")) {
        verbose = true;
    }

    // Create a new Converter and give it a plugin configuration
    std::shared_ptr<PfileToIsmrmrd::GERawConverter> converter;
    try {
        converter = std::make_shared<PfileToIsmrmrd::GERawConverter>(rawFile, verbose);
    } catch (const std::exception& e) {
        std::cerr << "Failed to instantiate converter: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // if a library and classname are specified, they take precedence over the config file
    if (libpath.size() > 0 || classname.size() > 0) {
        if (libpath.size() == 0) {
            std::cerr << "Must specify shared library path when overriding plugin" << std::endl;
            return EXIT_FAILURE;
        } else if (classname.size() == 0) {
            std::cerr << "Must specify plugin class name when overriding plugin" << std::endl;
            return EXIT_FAILURE;
        }

        try {
            converter->useConverter(classname);
        } catch (const std::exception& e) {
            std::cerr << "Failed to override plugin: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

    } else {
        try {
            std::cout << "Using configuration: " << configfile << std::endl;
            converter->useConfigFilename(configfile);
        } catch (const std::exception& e) {
            std::cerr << "Failed to use configuration: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Override stylesheet if specified
    if (stylesheet.size() > 0) {
        try {
            converter->useStylesheetFilename(stylesheet);
        } catch (const std::exception& e) {
            std::cerr << "Failed to override stylesheet: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Get the ISMRMRD Header String
    std::string xml_header;
    try {
        xml_header = converter->getIsmrmrdXMLHeader();
    } catch (const std::exception& e) {
        std::cerr << "Failed to get header string: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (xml_header.size() == 0) {
        std::cerr << "Empty ISMRMRD XML header... Exiting" << std::endl;
        return EXIT_FAILURE;
    }

    // if the user requested only a dump of the XML header:
    if (vm.count("string")) {
        std::cout << xml_header << std::endl;
        return EXIT_SUCCESS;
    }

    // create hdf5 file
    ISMRMRD::Dataset d(outfile.c_str(), "dataset", true);

    // write the ISMRMRD header to the dataset
    d.writeHeader(xml_header);

    // get the first acquisition in this raw file
    std::vector<ISMRMRD::Acquisition> acqs = converter->getAcquisitions(0);

    std::cout << "Number of acquisitions stored in HDF5 file is " << acqs.size() << std::endl;
    // add these acquisitions to the hdf5 dataset
    for (int n = 0; n < acqs.size(); n++) {
        d.appendAcquisition(acqs.at(n));
    }

    std::cout << "Swedished!" << std::endl;

    return EXIT_SUCCESS;
}

