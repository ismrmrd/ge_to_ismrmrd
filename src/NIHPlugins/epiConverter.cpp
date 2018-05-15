
/** @file 2dfastConverter.cpp */

#include "epiConverter.h"


std::vector<ISMRMRD::Acquisition> NIHepiConverter::getAcquisitions(GERecon::Legacy::Pfile* pfile,
                                                                   unsigned int acqMode)
{
   std::cerr << "Currently, conversion of EPI P-files is __NOT__ supported." << std::endl;

   exit (EXIT_FAILURE);
}



std::vector<ISMRMRD::Acquisition> NIHepiConverter::getAcquisitions(GERecon::ScanArchive* scanArchive,
                                                                   unsigned int acqMode)
{
   std::vector<ISMRMRD::Acquisition> acqs;

   std::cerr << "Currently, using EPI ScanArchive converter." << std::endl;

   GERecon::ScanArchivePointer scanArchivePtr(scanArchive);
   boost::filesystem::path scanArchivePath = scanArchivePtr->Path();

   GERecon::Acquisition::ArchiveStoragePointer archiveStoragePointer    = GERecon::Acquisition::ArchiveStorage::Create(scanArchivePtr);
   GERecon::Legacy::LxDownloadDataPointer lxData                        = boost::dynamic_pointer_cast<GERecon::Legacy::LxDownloadData>(scanArchive->LoadDownloadData());
   boost::shared_ptr<GERecon::Epi::LxControlSource> const controlSource = boost::make_shared<GERecon::Epi::LxControlSource>(lxData);
   GERecon::Control::ProcessingControlPointer processingControl         = controlSource->CreateOrchestraProcessingControl();

   // GERecon::Path::InputAppData(scanArchivePath.parent_path());
   // GERecon::Path::InputExamData(scanArchivePath.parent_path());
   // GERecon::Path::ScannerConfig(scanArchivePath.parent_path());
   scanArchivePtr->LoadSavedFiles();

   int const   packetQuantity = archiveStoragePointer->AvailableControlCount();

   int            packetCount = 0;
   int              dataIndex = 0;
   int                acqType = 0;
   unsigned int       nEchoes = processingControl->Value<int>("NumEchoes");
   unsigned int     nChannels = processingControl->Value<int>("NumChannels");
   unsigned int     numSlices = processingControl->Value<int>("NumSlices");
   size_t          frame_size = processingControl->Value<int>("AcquiredXRes");
   int const         topViews = processingControl->Value<int>("ExtraFramesTop");
   int const             yAcq = processingControl->Value<int>("AcquiredYRes");
   int const      bottomViews = processingControl->Value<int>("ExtraFramesBottom");
   // unsigned int     nRefViews = processingControl->Value<int>("NumRefViews"); // Variable not found at run time
   unsigned int     nRefViews = topViews + bottomViews;
 
   const RowFlipParametersPointer rowFlipper = boost::make_shared<RowFlipParameters>(yAcq + nRefViews);
   RowFlipPlugin rowFlipPlugin(rowFlipper, *processingControl);

   while (packetCount < packetQuantity)
   {
      // encoding IDs to fill ISMRMRD headers.
      int   sliceID = 0;
      int    viewID = 0;
      int  viewSkip = 0;
      int viewStart = 0;
      int   viewEnd = 0;

      GERecon::Acquisition::FrameControlPointer const thisPacket = archiveStoragePointer->NextFrameControl();

      // Need to identify opcode(s) here that will mark acquisition / reference / control
      if (thisPacket->Control().Opcode() != GERecon::Acquisition::ScanControlOpcode)
      {
         // For EPI scans, packets are now HyperFrameControl type
         GERecon::Acquisition::HyperFrameControlPacket const packetContents = thisPacket->Control().Packet().As<GERecon::Acquisition::HyperFrameControlPacket>();

         // viewID   =           GERecon::Acquisition::GetPacketValue(packetContents.viewNumH,  packetContents.viewNumL);
         viewSkip = static_cast<short>(Acquisition::GetPacketValue(packetContents.viewSkipH, packetContents.viewSkipL));
         sliceID  =           GERecon::Acquisition::GetPacketValue(packetContents.sliceNumH, packetContents.sliceNumL);

         acqType = GERecon::Acquisition::ImageFrame;

         acqs.resize(dataIndex + 1);

         auto kData = thisPacket->Data();
         ComplexFloatCube kRefData(frame_size, nRefViews, nChannels);
         ComplexFloatCube kImgData(frame_size, yAcq, nChannels);
         Range kRefDataRange = Range(0, nRefViews - 1);
         Range kImgDataRange = Range(nRefViews, nRefViews + yAcq - 1);

         std::cerr << "Num refViews is: " << nRefViews << std::endl;

         if (nRefViews > 0)
         {
            if (topViews > 0)
            {
               kRefDataRange   = Range(fromStart, topViews - 1);
               viewStart       = topViews;
               viewEnd         = topViews + yAcq - 1;
            }
            else if (bottomViews > 0)
            {
               kRefDataRange   = Range(yAcq, bottomViews + yAcq - 1);
               viewStart       = 0;
               viewEnd         = yAcq - 1;
            }
            else
               kRefDataRange = Range();

            kImgDataRange = Range(viewStart, viewEnd);

            for (int channelID = 0 ; channelID < nChannels ; channelID++)
            {
               kRefData(Range::all(), Range::all(), channelID) = kData(Range::all(), channelID, kRefDataRange);
               ComplexFloatMatrix tempRefData = kRefData(Range::all(), Range::all(), channelID);
               rowFlipPlugin.ApplyReferenceDataRowFlip(tempRefData);
            }
         }

         for (int channelID = 0 ; channelID < nChannels ; channelID++)
         {
            int phaseEncodeExtent = kData.extent(2);

            std::cerr << "Max phase encode value is: " << phaseEncodeExtent << std::endl;
            std::cerr << "Skip phase encode value is: " << viewSkip << std::endl;

            kImgData(Range::all(), Range::all(), channelID) = kData(Range::all(), channelID, kImgDataRange);
            ComplexFloatMatrix tempKImageData = kImgData(Range::all(), Range::all(), channelID);
            rowFlipPlugin.ApplyImageDataRowFlip(tempKImageData);

            for (viewID = viewStart ; viewID < viewEnd ; viewID++)
            {
               std::cerr << "Processing phase encode value : " << viewID << std::endl;

               // Grab a reference to the acquisition
               ISMRMRD::Acquisition& acq = acqs.at(dataIndex);

               // Set size of this data frame to receive raw data
               acq.resize(frame_size, nChannels, 0);

               for (int i = 0 ; i < frame_size ; i++)
               {
                  // The last dimension here in kData denotes the view
                  // index in the control packet that one must stride
                  // through to get data.  TODO - figure out if this
                  // can be programatically determined, and if so, use
                  // it. Will be needed for cases where multiple lines
                  // of data are contained in a single packet.
                  acq.data(i, channelID) = tempKImageData(i, channelID, viewID);
               }

               // Initialize the encoding counters for this acquisition.
               ISMRMRD::EncodingCounters idx;
               get_view_idx(processingControl, 0, idx);

               idx.slice                  = sliceID;
               idx.contrast               = 1;
               idx.kspace_encode_step_1   = viewID - 1;

               acq.idx() = idx;

               // Fill in the rest of the header
               acq.clearAllFlags();
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

               acq.setChannelActive(channelID);

               // Set first acquisition flag
               if (idx.kspace_encode_step_1 == 0)
                  acq.setFlag(ISMRMRD::ISMRMRD_ACQ_FIRST_IN_SLICE);

               // Set last acquisition flag
               if (idx.kspace_encode_step_1 == yAcq - 1)
                  acq.setFlag(ISMRMRD::ISMRMRD_ACQ_LAST_IN_SLICE);

               dataIndex++;
            }
         }
      }

      packetCount++;
   }

   return acqs;
}

SEQUENCE_CONVERTER_FACTORY_DECLARE(NIHepiConverter)

