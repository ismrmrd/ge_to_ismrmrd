
/** @file NIHepiConverter.cpp */

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
   std::cerr << "Using NIHepi ScanArchive converter." << std::endl;

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

   int const    packetQuantity = archiveStoragePointer->AvailableControlCount();

   int                 acqType = 0;
   unsigned int        nEchoes = processingControl->Value<int>("NumEchoes");
   unsigned int      nChannels = processingControl->Value<int>("NumChannels");
   unsigned int      numSlices = processingControl->Value<int>("NumSlices");
   size_t           frame_size = processingControl->Value<int>("AcquiredXRes");
   int const          topViews = processingControl->Value<int>("ExtraFramesTop");
   int const              yAcq = processingControl->Value<int>("AcquiredYRes");
   int const       bottomViews = processingControl->Value<int>("ExtraFramesBottom");
   // unsigned int     nRefViews = processingControl->Value<int>("NumRefViews"); // Variable not found at run time
   unsigned int      nRefViews = topViews + bottomViews;

   bool isEpiRefScanIntegrated = processingControl->Value<bool>("IntegratedReferenceScan");
   bool     isMultiBandEnabled = processingControl->Value<bool>("MultibandEnabled");
 
   int                nVolumes = processingControl->Value<int>("NumAcquisitions");
   int      nAcqsPerRepetition = processingControl->Value<int>("NumAcquisitionsPerRepetition");
   // float         acqSampleTime = processingControl->Value<float>("A2DSampleTime"); // does not exist in the Epi::LxControlSource object

   const RowFlipParametersPointer rowFlipper = boost::make_shared<RowFlipParameters>(yAcq + nRefViews);
   RowFlipPlugin rowFlipPlugin(rowFlipper, *processingControl);

   int dataIndex = 0;
   std::vector<ISMRMRD::Acquisition> acqs;

   for (int packetCount=0; packetCount < packetQuantity; packetCount++)
   {
      // encoding IDs to fill ISMRMRD headers.
      int   sliceID = 0;
      int    viewID = 0;
      int  viewSkip = 0;

      GERecon::Acquisition::FrameControlPointer const thisPacket = archiveStoragePointer->NextFrameControl();

      // Need to identify opcode(s) here that will mark acquisition / reference / control
      if (thisPacket->Control().Opcode() != GERecon::Acquisition::ScanControlOpcode)
      {
         // For EPI scans, packets are now HyperFrameControl type
         GERecon::Acquisition::HyperFrameControlPacket const packetContents = thisPacket->Control().Packet().As<GERecon::Acquisition::HyperFrameControlPacket>();

         viewSkip = static_cast<short>(Acquisition::GetPacketValue(packetContents.viewSkipH, packetContents.viewSkipL));
         sliceID  =           GERecon::Acquisition::GetPacketValue(packetContents.sliceNumH, packetContents.sliceNumL);

         acqType = GERecon::Acquisition::ImageFrame;
	 auto pktData = thisPacket->Data();

	 // Transpose the pktData - swapping channel (2) and phase (1) dimensions. This does not move data around in
         // memory - this just manipulates the strides.
	 pktData.transposeSelf( 0, 2, 1 );

	 // Flip Y dimension (for all x samples and all channels)
	 if (viewSkip < 0) {
	    pktData.reverseSelf(1);
	    // std::cout << "Data was FLIPPED alonig y-axis using reverseSelf()\n";
	 }

	 // Copy (and sort) the packet data into a new array, kdata.
	 ComplexFloatCube kData( pktData.shape() );
	 kData = pktData;

	 // Do the row-flipping.
	 //
	 // Note: Using ApplyImageDataRowFlip seems to work for all
	 // rows (image and reference)
         for (int channelID = 0 ; channelID < nChannels ; channelID++)
         {
	   ComplexFloatMatrix tempData = kData(Range::all(), Range::all(), channelID);
	   rowFlipPlugin.ApplyImageDataRowFlip(tempData);
         }

         // Copy data out of ScanArchive into ISMRMRD object
	 int totalViews = topViews + yAcq + bottomViews;

         for (int view = 0; view < totalViews; ++view)
         {
            // Create a new acquisition object for this chunk of data
            acqs.resize(dataIndex + 1);

            // Grab a reference to the acquisition
            ISMRMRD::Acquisition &acq = acqs.at(dataIndex);

            // Set size of this data frame to receive raw data
            acq.resize(frame_size, nChannels, 0);
            acq.clearAllFlags();

            // Initialize the encoding counters for this acquisition.
            ISMRMRD::EncodingCounters &idx = acq.idx();
            get_view_idx(processingControl, 0, idx);
            idx.slice                  = sliceID;
            idx.contrast               = 1;          // JAD: Should this be 0?

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

            // Set first acquisition flag
            if (view == 0)
               acq.setFlag(ISMRMRD::ISMRMRD_ACQ_FIRST_IN_SLICE);

            // Set last acquisition flag
            if (view == totalViews - 1)
               acq.setFlag(ISMRMRD::ISMRMRD_ACQ_LAST_IN_SLICE);

	    if (view < topViews || view >= topViews + yAcq) {
	       // This is a reference view
               acq.setFlag(ISMRMRD::ISMRMRD_ACQ_IS_PHASECORR_DATA);
	       idx.kspace_encode_step_1 = yAcq/2;
	    }
	    else {
	       // This view is an image view
	       idx.kspace_encode_step_1 = view - topViews;
	    }

	    // Copy view data to ISMRMRD Acq data packet
            for (int channelID = 0 ; channelID < nChannels ; channelID++)
            {
               for (int x = 0 ; x < frame_size ; x++)
               {
                  acq.data(x, channelID) = kData(x, view, channelID);
               }
               acq.setChannelActive(channelID);
            }

            dataIndex++;
         }
      }
   }
   return acqs;
}

SEQUENCE_CONVERTER_FACTORY_DECLARE(NIHepiConverter)

