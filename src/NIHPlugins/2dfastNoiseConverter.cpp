
/** @file 2dfastNoiseConverter.cpp */

#include "2dfastNoiseConverter.h"

std::vector<ISMRMRD::Acquisition> NIH2dfastNoiseConverter::getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
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

                    for (int i = 0 ; i < frame_size ; i++)
                    {
                       acq.data(i, channelID) = kData(i, phaseCount);
                    }
                }

                // Initialize the encoding counters for this acquisition.
                ISMRMRD::EncodingCounters idx;
                get_view_idx(processingControl, 0, idx);

                idx.slice = sliceCount;
                idx.contrast  = echoCount;
                idx.kspace_encode_step_1 = phaseCount;

                acq.idx() = idx;

                // Fill in the rest of the header
                acq.scan_counter() = acq_num;
                acq.available_channels() = nChannels;
                acq.center_sample() = frame_size/2;

                for (int ch = 0 ; ch < nChannels ; ch++) {
                    acq.setChannelActive(ch);
                }

                // Since this is the noise converter - set Noise flag
                acq.setFlag(ISMRMRD::ISMRMRD_ACQ_IS_NOISE_MEASUREMENT);

                acq_num++;
            } // end of phaseCount loop
        } // end of echoCount loop
    } // end of sliceCount loop

    return acqs;
}



std::vector<ISMRMRD::Acquisition> NIH2dfastNoiseConverter::getAcquisitions(GERecon::ScanArchivePointer &scanArchivePtr,
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

   while (packetCount < packetQuantity)
   {
      // encoding IDs to fill ISMRMRD headers.
      int   sliceID = 0;
      int    viewID = 0;

      GERecon::Acquisition::FrameControlPointer const thisPacket = archiveStoragePointer->NextFrameControl();

      // Need to identify opcode(s) here that will mark acquisition / reference / control
      if (thisPacket->Control().Opcode() != GERecon::Acquisition::ScanControlOpcode)
      {
         GERecon::Acquisition::ProgrammableControlPacket const packetContents = thisPacket->Control().Packet().As<GERecon::Acquisition::ProgrammableControlPacket>();

         viewID  = GERecon::Acquisition::GetPacketValue(packetContents.viewNumH,  packetContents.viewNumL);
         sliceID = GERecon::Acquisition::GetPacketValue(packetContents.sliceNumH, packetContents.sliceNumL);

         if ((viewID < 1) || (viewID > nPhases))
         {
            acqType = GERecon::Acquisition::BaselineFrame;
            // nothing else to be done here for basic 2D case
         }
         else
         {
            acqType = GERecon::Acquisition::ImageFrame;

            acqs.resize(dataIndex + 1);

            auto kData = thisPacket->Data();

            // Grab a reference to the acquisition
            ISMRMRD::Acquisition& acq = acqs.at(dataIndex);

            // Set size of this data frame to receive raw data
            acq.resize(frame_size, nChannels, 0);
            acq.clearAllFlags();

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

            // Initialize the encoding counters for this acquisition.
            ISMRMRD::EncodingCounters idx;
            get_view_idx(processingControl, 0, idx);

            idx.slice                  = sliceID;
            idx.contrast               = 1;
            idx.kspace_encode_step_1   = viewID - 1;

            acq.idx() = idx;

            // Fill in the rest of the header
            acq.scan_counter()         = dataIndex;
            acq.available_channels()   = nChannels;
            acq.center_sample()        = frame_size/2;

            for (int ch = 0 ; ch < nChannels ; ch++) {
               acq.setChannelActive(ch);
            }

            // Since this is the noise converter - set Noise flag
            acq.setFlag(ISMRMRD::ISMRMRD_ACQ_IS_NOISE_MEASUREMENT);

            dataIndex++;
         }
      }

      packetCount++;
   }

   return acqs;
}

SEQUENCE_CONVERTER_FACTORY_DECLARE(NIH2dfastNoiseConverter)

