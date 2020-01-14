
/** @file QGenericConverter.cpp */
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "QGenericConverter.h"

struct LOADTEST {
   LOADTEST() { std::cerr << __FILE__ << ": shared object loaded"   << std::endl; }
  ~LOADTEST() { std::cerr << __FILE__ << ": shared object unloaded" << std::endl; }
} loadTest;

namespace PfileToIsmrmrd {

QGenericConverter::QGenericConverter() : SequenceConverter()
{
   
}

// use the psdname to determine if the converter should infer the slice indices rather than
// use the index stored in the header
// returns:
// true if the psdname matches a sequence where it is known that the packets don't store the slice index
// false if the psdname is not one of the above
bool QGenericConverter::isSliceIndexInferred(std::string& psdname) const
{
   // throw exception if the psdname is empty

   // match strings
   // ssfse/HASTE
   if(psdname.compare("ssfse") == 0)
      return true;

   else
      return false;
   
}


int QGenericConverter::get_view_idx(GERecon::Control::ProcessingControlPointer processingControl,
                                   unsigned int view_num, ISMRMRD::EncodingCounters &idx)
{
    // set all the ones we don't care about to zero
    idx.kspace_encode_step_2 = 0;
    idx.average = 0;
    idx.contrast = 0;
    idx.phase = 0;
    idx.set = 0;
    idx.segment = 0;
    for (int n=0; n<8; n++) {
        idx.user[n] = 0;
    }

    unsigned int nframes   = processingControl->Value<int>("AcquiredYRes");
    unsigned int numSlices = processingControl->Value<int>("NumSlices");

    idx.repetition = view_num / (numSlices * (1 + nframes));

    if (view_num < 1) {
        // this is the mean baseline view return -1
        return -1;
    }

    return 1;
}



std::vector<ISMRMRD::Acquisition> QGenericConverter::getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
                                                                    unsigned int acqMode)
{
    std::vector<ISMRMRD::Acquisition> acqs;

    const GERecon::Control::ProcessingControlPointer processingControl(pfile->CreateOrchestraProcessingControl());
    unsigned int nPhases   = processingControl->Value<int>("AcquiredYRes");
    unsigned int nEchoes   = processingControl->Value<int>("NumEchoes");
    unsigned int nChannels = processingControl->Value<int>("NumChannels");
    unsigned int numSlices = processingControl->Value<int>("NumSlices");

    // Make number of acquisitions to be converted
    acqs.resize(numSlices * nEchoes * nPhases);

    unsigned int acq_num = 0;

    // Orchestra API provides size in bytes.
    // frame_size is the number of complex points in a single channel
    size_t frame_size = processingControl->Value<int>("AcquiredXRes");

    for (int sliceCount = 0 ; sliceCount < numSlices ; sliceCount++)
    {
        for (int echoCount = 0 ; echoCount < nEchoes ; echoCount++)
        {
            for (int phaseCount = 0 ; phaseCount < nPhases ; phaseCount++)
            {
                // Grab a reference to the acquisition
                ISMRMRD::Acquisition& acq = acqs.at(acq_num);

                // Set size of this data frame to receive raw data
                acq.resize(frame_size, nChannels, 0);
                acq.clearAllFlags();

                // Initialize the encoding counters for this acquisition.
                ISMRMRD::EncodingCounters idx;
                get_view_idx(processingControl, 0, idx);

                idx.slice = sliceCount;
                idx.contrast  = echoCount;
                idx.kspace_encode_step_1 = phaseCount;

                acq.idx() = idx;

                // Fill in the rest of the header
                // acq.measurement_uid() = pfile->RunNumber();
                acq.scan_counter() = acq_num;
                acq.acquisition_time_stamp() = time(NULL); // TODO: can we get a timestamp?
                for (int p=0; p<ISMRMRD::ISMRMRD_PHYS_STAMPS; p++) {
                    acq.physiology_time_stamp()[p] = 0;
                }
                acq.available_channels() = nChannels;
                acq.discard_pre() = 0;
                acq.discard_post() = 0;;
                acq.center_sample() = frame_size/2;
                acq.encoding_space_ref() = 0;
                //acq.sample_time_us() = pfile->sample_time * 1e6;

                for (int ch = 0 ; ch < nChannels ; ch++) {
                    acq.setChannelActive(ch);
                }

                // Patient table off-center
                // TODO: fix the patient table position
                acq.patient_table_position()[0] = 0.0;
                acq.patient_table_position()[1] = 0.0;
                acq.patient_table_position()[2] = 0.0;

                // Slice position and orientation
                /* TODO
                static pfile_slice_vectors_t slice_vectors;
                pfile_get_slice_vectors(pfile, idx.slice, &slice_vectors);

                acq.read_dir()[0] = slice_vectors.read_dir.x;
                acq.read_dir()[1] = slice_vectors.read_dir.y;
                acq.read_dir()[2] = slice_vectors.read_dir.z;
                acq.phase_dir()[0] = slice_vectors.phase_dir.x;
                acq.phase_dir()[1] = slice_vectors.phase_dir.y;
                acq.phase_dir()[2] = slice_vectors.phase_dir.z;
                acq.slice_dir()[0] = slice_vectors.slice_dir.x;
                acq.slice_dir()[1] = slice_vectors.slice_dir.y;
                acq.slice_dir()[2] = slice_vectors.slice_dir.z;
                acq.position()[0] = slice_vectors.center.x;
                acq.position()[1] = slice_vectors.center.y;
                acq.position()[2] = slice_vectors.center.z;
                */

                // Set first acquisition flag
                if (idx.kspace_encode_step_1 == 0)
                    acq.setFlag(ISMRMRD::ISMRMRD_ACQ_FIRST_IN_SLICE);

                // Set last acquisition flag
                if (idx.kspace_encode_step_1 == nPhases - 1)
                    acq.setFlag(ISMRMRD::ISMRMRD_ACQ_LAST_IN_SLICE);

                // Get data from P-file using KSpaceData object, and copy
                // into ISMRMRD space.
                for (int channelID = 0 ; channelID < nChannels ; channelID++)
                {
                    // VR + JAD - 2016.01.15 - looking at various schemes to stride and read in
                    // K-space data.
                    //
                    // ViewData - will read in "acquisitions", including baselines, starting at
                    //            index 0, going up to slices * echo * (view + baselines)
                    //
                    // KSpaceData (slice, echo, channel, phase = 0) - reads in data, assuming "GE
                    //            native" data order in P-file, gives one slice / image worth of
                    //            K-space data, with baseline views automagically excluded.
                    //
                    // KSpaceData can return different numerical data types.  Picked float to
                    // be consistent with ISMRMRD data type.  This implementation of KSpaceData
                    // is used for data acquired in the "native" GE order.

                    auto kData = pfile->KSpaceData<float>(sliceCount, echoCount, channelID);

                    if (processingControl->Value<bool>("ChopY") == 0) {
                       if (idx.kspace_encode_step_1 % 2 == 1) {
                          kData *= -1.0f;
                       }
                    }

                    for (int i = 0 ; i < frame_size ; i++)
                    {
                       acq.data(i, channelID) = kData(i, phaseCount);
                    }
                }

                acq_num++;
            } // end of phaseCount loop
        } // end of echoCount loop
    } // end of sliceCount loop

    return acqs;
}

/**
*
* @throws std::runtime_error { if plugin encounters data it cannot handle }
*/
std::vector<ISMRMRD::Acquisition> QGenericConverter::getAcquisitions(GERecon::ScanArchivePointer &scanArchivePtr,
                                                                    unsigned int acqMode)
{
   std::vector<ISMRMRD::Acquisition> acqs;

   GERecon::Acquisition::ArchiveStoragePointer archiveStoragePointer = GERecon::Acquisition::ArchiveStorage::Create(scanArchivePtr);
   GERecon::Legacy::LxDownloadDataPointer lxData = boost::dynamic_pointer_cast<GERecon::Legacy::LxDownloadData>(scanArchivePtr->LoadDownloadData());
   boost::shared_ptr<GERecon::Legacy::LxControlSource> const controlSource = boost::make_shared<GERecon::Legacy::LxControlSource>(lxData);
   GERecon::Control::ProcessingControlPointer processingControl = controlSource->CreateOrchestraProcessingControl();

   int const   packetQuantity = archiveStoragePointer->AvailableControlCount();

   int            packetCount = 0;
   int              dataIndex = 0;
   int                acqType = 0;
   unsigned int       nPhases = processingControl->Value<int>("AcquiredYRes");
   unsigned int       nEchoes = processingControl->Value<int>("NumEchoes");
   unsigned int     nChannels = processingControl->Value<int>("NumChannels");
   unsigned int     numSlices = processingControl->Value<int>("NumSlices");
   size_t          frame_size = processingControl->Value<int>("AcquiredXRes");

   std::cout << "PSDname: " << const_cast<const GERecon::Legacy::LxDownloadData&>(*lxData).ImageHeaderData().psdname << std::endl;

   // TW: Some Slice debugging here with lots of noise
   std::cout << "numSlices = " << numSlices << std::endl;


   while (packetCount < packetQuantity)
   {
      // encoding IDs to fill ISMRMRD headers.
      int   sliceID = 0;
      int    viewID = 0;

      GERecon::Acquisition::FrameControlPointer const thisPacket = archiveStoragePointer->NextFrameControl();

      // Need to identify opcode(s) here that will mark acquisition / reference / control
      if (thisPacket->Control().Opcode() != GERecon::Acquisition::ScanControlOpcode)
      {

         // this is an issue here, because this limits this to ProgrammableControlPackets
         GERecon::Acquisition::ProgrammableControlPacket const packetContents = thisPacket->Control().Packet().As<GERecon::Acquisition::ProgrammableControlPacket>();

         viewID  = GERecon::Acquisition::GetPacketValue(packetContents.viewNumH,  packetContents.viewNumL);
         sliceID = GERecon::Acquisition::GetPacketValue(packetContents.sliceNumH, packetContents.sliceNumL);

         // output something if the opcode is not 1, to make it less noisy
         if(static_cast<uint8_t>(packetContents.opcode) != GERecon::Acquisition::ProgrammableOpcode)
            std::cout << "viewID = " << viewID << " sliceID = " << sliceID << " opcode = " << (int)(packetContents.opcode) << std::endl << std::flush;
         

         //std::cout << "sliceID = " << sliceID << " (" << static_cast<int>(packetContents.sliceNumH) << "," << static_cast<int>(packetContents.sliceNumL) << ")" << std::endl;
         if(static_cast<uint8_t>(packetContents.opcode) == GERecon::Acquisition::ProgrammableOpcode) {

            GERecon::Acquisition::CartesianFrameCommand F(packetContents);
            F.Dump(std::cout);
         }
         // TW: temporary hack, ignore the ViewCopy opcode
         //     I'm undecided on strategy here, but Packets with no data cause:
         //     - undefined behaviour/segfault on the ->Data() call (Boo GE)
         //     - we have nothing to write into the ISMRMRD file either, when we have no data 
         if(static_cast<uint8_t>(packetContents.opcode) != GERecon::Acquisition::ViewCopyOpcode) {
            if ((viewID < 1) || (viewID > nPhases))
            {
               acqType = GERecon::Acquisition::BaselineFrame;
               // nothing else to be done here for basic 2D case
            }    
            else
            {
               acqType = GERecon::Acquisition::ImageFrame;

               acqs.resize(dataIndex + 1);

               // TW build in some segmentation fault protection here, as Orchestra WILL segfault on the ->Data() call
               //    I'm interpreting a 0 rval as meaning that there is no kspace data in the packet as the 
               //    header states that the data size is [FrameSize x NumChannels x NumFrames] 
               if(thisPacket->NumFrames() == 0) {
                  throw std::runtime_error("Attempting to access data of a packet that has no data.");                  
               }
 
               // TW: Now we are seeing some C++11 suddenly some type inference
               auto kData = thisPacket->Data();

               // Grab a reference to the acquisition
               ISMRMRD::Acquisition& acq = acqs.at(dataIndex);

               // Set size of this data frame to receive raw data
               acq.resize(frame_size, nChannels, 0);
               acq.clearAllFlags();

               // Initialize the encoding counters for this acquisition.
               ISMRMRD::EncodingCounters idx;
               get_view_idx(processingControl, 0, idx);

               idx.slice                  = sliceID;
               idx.contrast               = packetContents.echoNum;
               idx.kspace_encode_step_1   = viewID - 1;

               acq.idx() = idx;

               // Fill in the rest of the header
               // acq.measurement_uid() = pfile->RunNumber();
               acq.scan_counter() = dataIndex;
               acq.acquisition_time_stamp() = time(NULL);
               for (int p=0; p<ISMRMRD::ISMRMRD_PHYS_STAMPS; p++) {
                  acq.physiology_time_stamp()[p] = 0;
               }
               acq.available_channels()   = nChannels;
               acq.discard_pre()          = 0;
               acq.discard_post()         = 0;
               acq.center_sample()        = frame_size/2;
               acq.encoding_space_ref()   = 0;
               // acq.sample_time_us()       = pfile->sample_time * 1e6;

               for (int ch = 0 ; ch < nChannels ; ch++) {
                  acq.setChannelActive(ch);
               }

               // Set first acquisition flag
               if (idx.kspace_encode_step_1 == 0)
                  acq.setFlag(ISMRMRD::ISMRMRD_ACQ_FIRST_IN_SLICE);

               // Set last acquisition flag
               if (idx.kspace_encode_step_1 == nPhases - 1)
                  acq.setFlag(ISMRMRD::ISMRMRD_ACQ_LAST_IN_SLICE);

               if (processingControl->Value<bool>("ChopY") == 0) {
                  if (idx.kspace_encode_step_1 % 2 == 1) {
                     kData *= -1.0f;
                  }
               }

               for (int channelID = 0 ; channelID < nChannels ; channelID++)
               {
                  for (int i = 0 ; i < frame_size ; i++)
                  {
                     // The last dimension here in kData denotes the view
                     // index in the control packet that one must stride
                     // through to get data.  TODO - figure out if this
                     // can be programatically determined, and if so, use
                     // it. Will be needed for cases where multiple lines
                     // of data are contained in a single packet.
		               acq.data(i, channelID) = kData(i, channelID, 0);
                  }
               }

               dataIndex++;
            }
         } // if(static_cast<uint8_t>(packetContents.opcode) == 1)
      }

      packetCount++;
   }

   return acqs;
}

SEQUENCE_CONVERTER_FACTORY_DECLARE(QGenericConverter)

} // namespace PfileToIsmrmrd

