
/** @file NIHepiConverter.cpp */

#include "epiConverter.h"


std::vector<ISMRMRD::Acquisition> NIHepiConverter::getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
                                                                   unsigned int acqMode)
{
   std::cerr << "Currently, conversion of EPI P-files is __NOT__ supported." << std::endl;

   exit (EXIT_FAILURE);
}



std::vector<ISMRMRD::Acquisition> NIHepiConverter::getAcquisitions(GERecon::ScanArchivePointer &scanArchivePtr,
                                                                   unsigned int acqMode)
{
   std::cerr << "Using NIHepi ScanArchive converter." << std::endl;

   boost::filesystem::path scanArchivePath = scanArchivePtr->Path();

   GERecon::Acquisition::ArchiveStoragePointer archiveStoragePointer    = GERecon::Acquisition::ArchiveStorage::Create(scanArchivePtr);
   GERecon::Legacy::LxDownloadDataPointer lxData                        = boost::dynamic_pointer_cast<GERecon::Legacy::LxDownloadData>(scanArchivePtr->LoadDownloadData());
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
 
   // Commented out for now, as these don't seem to hold necessary values for EPI.
   // int                nVolumes = processingControl->Value<int>("NumAcquisitions");
   // int      nAcqsPerRepetition = processingControl->Value<int>("NumAcquisitionsPerRepetition");
   // float         acqSampleTime = processingControl->Value<float>("A2DSampleTime"); // does not exist in the Epi::LxControlSource object

   const RowFlipParametersPointer rowFlipper = boost::make_shared<RowFlipParameters>(yAcq + nRefViews);
   RowFlipPlugin rowFlipPlugin(rowFlipper, *processingControl);

   int dataIndex = 0;
   std::vector<ISMRMRD::Acquisition> acqs;

   Range refViewsRange;
   int   refViewsStart, refViewsEnd;

   if (nRefViews > 0)
   {
      if (topViews > 0)
      {
         refViewsStart = 0;
         refViewsEnd   = topViews - 1;
      }
      else if (bottomViews > 0)
      {
         refViewsStart = yAcq;
         refViewsEnd   = yAcq + bottomViews - 1;
      }
      else
      {
         refViewsStart = 0;
         refViewsEnd   = 0;
      }
      refViewsRange = Range (refViewsStart, refViewsEnd);

      std::cout << "Reference views range: " << refViewsRange << std::endl;
      std::cout << "yAcq: " << yAcq << ", topViews: " << topViews << ", bottomViews: " << bottomViews << std::endl;
   }

   for (int packetCount=0; packetCount < packetQuantity; packetCount++)
   {
      GERecon::Acquisition::FrameControlPointer const thisPacket = archiveStoragePointer->NextFrameControl();

      // Need to identify opcode(s) here that will mark acquisition / reference / control
      if (thisPacket->Control().Opcode() != GERecon::Acquisition::ScanControlOpcode)
      {
         // For EPI scans, packets are now HyperFrameControl type
         GERecon::Acquisition::HyperFrameControlPacket const packetContents = thisPacket->Control().Packet().As<GERecon::Acquisition::HyperFrameControlPacket>();

         const GERecon::SliceInfoTable sliceTable = processingControl->ValueStrict<GERecon::SliceInfoTable>("SliceTable");

         int viewSkip = static_cast<short>(Acquisition::GetPacketValue(packetContents.viewSkipH, packetContents.viewSkipL));

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

         // Unchop RF-chopped data
         kData(Range::all(), Range(fromStart, toEnd, 2), Range::all()) *= -1.0f;

         // Copy data out of ScanArchive into ISMRMRD object
         int totalViews = topViews + yAcq + bottomViews;

         // Pre-allocate memory for incoming views
         acqs.resize(dataIndex + totalViews);

         int ref_count = 0;
         int pe1_index = 0;

         for (int view = 0; view < totalViews; ++view)
         {
            // Figure out where to put this view (i.e. effectively
            // re-sorting the views in the packet so that the reference
            // data comes first.

            int acq_index = 0;

            if ((nRefViews > 0) && (view >= refViewsStart) && (view <= refViewsEnd)) {
               // This view contains reference scan data
               pe1_index = yAcq/2;
               acq_index = dataIndex + ref_count++;
            }
            else {
               // This view constains (k-space) image data
               pe1_index = view - topViews;
               acq_index = dataIndex + nRefViews + pe1_index;
            }

            // Grab a reference to the acquisition
            ISMRMRD::Acquisition &acq = acqs.at(acq_index);

            // Set size of this data frame to receive raw data
            acq.resize(frame_size, nChannels, 0);
            acq.clearAllFlags();

            // Initialize the encoding counters for this acquisition.
            ISMRMRD::EncodingCounters &idx = acq.idx();

            idx.kspace_encode_step_1   = pe1_index;
            idx.slice                  = sliceTable.GeometricSliceNumber(GERecon::Acquisition::GetPacketValue(packetContents.sliceNumH,
                                                                                                              packetContents.sliceNumL));
            idx.repetition             = (int) (dataIndex / (numSlices * totalViews));
            idx.contrast               = packetContents.echoNum;

            // acq.measurement_uid() = pfile->RunNumber();
            acq.scan_counter()         = dataIndex + view;
            acq.acquisition_time_stamp() = time(NULL);
            for (int p=0; p<ISMRMRD::ISMRMRD_PHYS_STAMPS; p++) {
               acq.physiology_time_stamp()[p] = 0;
            }
            acq.available_channels()   = nChannels;
            acq.center_sample()        = frame_size/2;
            // acq.sample_time_us()       = pfile->sample_time * 1e6;

            // Set first acquisition flag
            if (view == 0)
               acq.setFlag(ISMRMRD::ISMRMRD_ACQ_FIRST_IN_SLICE);

            // Set last acquisition flag
            if (view == totalViews - 1)
               acq.setFlag(ISMRMRD::ISMRMRD_ACQ_LAST_IN_SLICE);

            // Label reference scan data
            // if (view < topViews || view >= topViews + yAcq)
            if ((nRefViews > 0) && (view >= refViewsStart) && (view <= refViewsEnd))
            {
               // std::cout << "Setting view: " << view << " as phase correction line." << std::endl;

               acq.setFlag(ISMRMRD::ISMRMRD_ACQ_IS_PHASECORR_DATA);
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

            setISMRMRDSliceVectors(processingControl, acq);
         }
         dataIndex += totalViews;
      }
   }
   return acqs;
}

SEQUENCE_CONVERTER_FACTORY_DECLARE(NIHepiConverter)

