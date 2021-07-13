
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
   std::string classname, stylesheet, rawFile, outfile;

   std::string thisProgram = argv[0];
   std::string validInputs = "input P- or ScanArchive File";
   std::string usage = thisProgram + " [options] <" + validInputs + ">";
   std::string stylesheet_default = get_ge_tools_home() + "share/ge-tools/config/default.xsl";
   std::string sequence_class_default = "GenericConverter";

   po::options_description basic("Basic Options");
   basic.add_options()
      ("help,h", "print help message")
      ("verbose,v", "enable verbose mode")
      ("plugin,p", po::value<std::string>(&classname)->default_value(sequence_class_default), "class/sequence name in library used for conversion")
      ("stylesheet,x", po::value<std::string>(&stylesheet)->default_value(stylesheet_default), "XSL stylesheet file mapping values provided by Orchestra to those needed by ISMRMRD")
      ("output,o", po::value<std::string>(&outfile)->default_value("converted_data.h5"), "output HDF5 file")
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
   std::shared_ptr<GEToIsmrmrd::GERawConverter> converter;
   try {
      converter = std::make_shared<GEToIsmrmrd::GERawConverter>(rawFile, classname, verbose);
   } catch (const std::exception& e) {
      std::cerr << "Failed to instantiate converter: " << e.what() << std::endl;
      return EXIT_FAILURE;
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

