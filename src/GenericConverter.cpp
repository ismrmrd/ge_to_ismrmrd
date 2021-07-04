
/** @file GenericConverter.cpp */
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "GenericConverter.h"

struct LOADTEST {
   LOADTEST() { std::cerr << __FILE__ << ": shared object loaded"   << std::endl; }
  ~LOADTEST() { std::cerr << __FILE__ << ": shared object unloaded" << std::endl; }
} loadTest;

namespace GEToIsmrmrd {

int GenericConverter::get_view_idx(GERecon::Control::ProcessingControlPointer processingControl,
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



std::vector<ISMRMRD::Acquisition> GenericConverter::getAcquisitions(GERecon::Legacy::PfilePointer &pfile,
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

                setISMRMRDSliceVectors(processingControl, acq);

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



std::vector<ISMRMRD::Acquisition> GenericConverter::getAcquisitions(GERecon::ScanArchivePointer &scanArchivePtr,
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
      unsigned int   sliceID = 0;
      unsigned int    viewID = 0;

      GERecon::Acquisition::FrameControlPointer const thisPacket = archiveStoragePointer->NextFrameControl();

      // Need to identify opcode(s) here that will mark acquisition / reference / control
      if (thisPacket->Control().Opcode() != GERecon::Acquisition::ScanControlOpcode)
      {
         GERecon::Acquisition::ProgrammableControlPacket const packetContents = thisPacket->Control().Packet().As<GERecon::Acquisition::ProgrammableControlPacket>();

         const GERecon::SliceInfoTable sliceTable = processingControl->ValueStrict<GERecon::SliceInfoTable>("SliceTable");

         viewID  = GERecon::Acquisition::GetPacketValue(packetContents.viewNumH,  packetContents.viewNumL);
         // Convert acquired slice index to spatial / geometric slice index
         sliceID = sliceTable.GeometricSliceNumber(GERecon::Acquisition::GetPacketValue(packetContents.sliceNumH, packetContents.sliceNumL));

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

            // Initialize the encoding counters for this acquisition.
            ISMRMRD::EncodingCounters idx;
            get_view_idx(processingControl, viewID, idx);

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

            setISMRMRDSliceVectors(processingControl, acq);

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
      }

      packetCount++;
   }

   return acqs;
}



int GenericConverter::setISMRMRDSliceVectors(GERecon::Control::ProcessingControlPointer processingControl,
                                             ISMRMRD::Acquisition& acq)
{
   static geRawDataSliceVectors_t sliceVectors;

   // Patient table off-center
   // TODO: fix the patient table position
   acq.patient_table_position()[0] = 0.0;
   acq.patient_table_position()[1] = 0.0;
   acq.patient_table_position()[2] = 0.0;

   getSliceVectors(processingControl, acq.idx().slice, &sliceVectors);

   acq.read_dir()[0]  = sliceVectors.read_dir.x;
   acq.read_dir()[1]  = sliceVectors.read_dir.y;
   acq.read_dir()[2]  = sliceVectors.read_dir.z;
   acq.phase_dir()[0] = sliceVectors.phase_dir.x;
   acq.phase_dir()[1] = sliceVectors.phase_dir.y;
   acq.phase_dir()[2] = sliceVectors.phase_dir.z;
   acq.slice_dir()[0] = sliceVectors.slice_dir.x;
   acq.slice_dir()[1] = sliceVectors.slice_dir.y;
   acq.slice_dir()[2] = sliceVectors.slice_dir.z;
   acq.position()[0]  = sliceVectors.center.x;
   acq.position()[1]  = sliceVectors.center.y;
   acq.position()[2]  = sliceVectors.center.z;

   return 0;
}



int GenericConverter::getSliceVectors(GERecon::Control::ProcessingControlPointer processingControl,
                                      unsigned int sliceNumber, geRawDataSliceVectors_t* vecs)
{
   float gwp1[3],   gwp2[3],   gwp3[3];
   float gwp1_0[3], gwp2_0[3], gwp3_0[3];

   const GERecon::SliceInfoTable sliceTable = processingControl->ValueStrict<GERecon::SliceInfoTable>("SliceTable");

   // const GERecon::ImageCorners imageCorners = GERecon::ImageCorners(sliceTable.AcquiredSliceCorners(sliceNumber),
                                                                    // sliceTable.SliceOrientation(sliceNumber));

   // std::cout << "Slice[" << sliceNumber << "] corners are:    " << sliceTable.AcquiredSliceCorners(sliceNumber) << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] orientation is: " <<   sliceTable.SliceOrientation(sliceNumber)   << std::endl;

   GERecon::SliceCorners sliceCorners = sliceTable.AcquiredSliceCorners(sliceNumber);

   // std::cout << "Slice[" << sliceNumber << "] UL   corner is:   " << sliceCorners.UpperLeft()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UR   corner is:   " << sliceCorners.UpperRight() << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] LL   corner is:   " << sliceCorners.LowerLeft()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UL.x corner is:   " << sliceCorners.UpperLeft().X_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UL.y corner is:   " << sliceCorners.UpperLeft().Y_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UL.z corner is:   " << sliceCorners.UpperLeft().Z_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UR.x corner is:   " << sliceCorners.UpperRight().X_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UR.y corner is:   " << sliceCorners.UpperRight().Y_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] UR.z corner is:   " << sliceCorners.UpperRight().Z_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] LL.x corner is:   " << sliceCorners.LowerLeft().X_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] LL.y corner is:   " << sliceCorners.LowerLeft().Y_mm()  << std::endl;
   // std::cout << "Slice[" << sliceNumber << "] LL.z corner is:   " << sliceCorners.LowerLeft().Z_mm()  << std::endl;

   /* TODO - need to make sure these are consistent with how they are treated in rotateVectorOnPatient function.
    *
    * These did not seem to be consisent.  'patientEntry' seemed okay though looked like it could get much more
    * complicated.  'patientPosition' required more checking.
    *

    * int patientEntry    = processingControl->Value<int>("PatientEntry")    - 1;
    * int patientPosition = processingControl->Value<int>("PatientPosition") - 1;

    * grab the gw_points from this slice's info entry
    * gwp1_0 = slice_info[sliceNumber].gw_point1;
    * gwp2_0 = slice_info[sliceNumber].gw_point2;
    * gwp3_0 = slice_info[sliceNumber].gw_point3;

    */

   int patientEntry, patientPosition;

   // std::cout << "Patient entry: "    << processingControl->Value<int>("PatientEntry")    << std::endl;
   // std::cout << "Patient position: " << processingControl->Value<int>("PatientPosition") << std::endl;

   switch (processingControl->Value<int>("PatientEntry"))
   {
      case 2 :
         patientEntry = 1;      /* Feet first */
         break;

      default :
         patientEntry = 0;      /* Head first */
   }

   switch (processingControl->Value<int>("PatientPosition"))
   {
      case 2 :
         patientPosition = 1;   /* Prone */
         break;

      case 4 :
         patientPosition = 2;   /* Left Decubitus */
         break;

      case 8 :
         patientPosition = 3;   /* Right Decubitus */
         break;

      default :
         patientPosition = 0;   /* Supine */
   }

   gwp1_0[0] = sliceCorners.UpperLeft().X_mm();
   gwp1_0[1] = sliceCorners.UpperLeft().Y_mm();
   gwp1_0[2] = sliceCorners.UpperLeft().Z_mm();

   gwp2_0[0] = sliceCorners.UpperRight().X_mm();
   gwp2_0[1] = sliceCorners.UpperRight().Y_mm();
   gwp2_0[2] = sliceCorners.UpperRight().Z_mm();

   gwp3_0[0] = sliceCorners.LowerLeft().X_mm();
   gwp3_0[1] = sliceCorners.LowerLeft().Y_mm();
   gwp3_0[2] = sliceCorners.LowerLeft().Z_mm();

   // rotate each coordinate according to the patient's position
   // this also puts the coordinates into DICOM/patient coordinate space
   rotateVectorOnPatient(patientEntry, patientPosition, gwp1_0, gwp1);
   rotateVectorOnPatient(patientEntry, patientPosition, gwp2_0, gwp2);
   rotateVectorOnPatient(patientEntry, patientPosition, gwp3_0, gwp3);

   // // Add the Z table offset back to each coordinate
   // // table_offset_z = image_hdr->ctr_S - (gwp3[2] + gwp2[2]) / 2;
   // gwp1[2] += pfile->table_offset_z;
   // gwp2[2] += pfile->table_offset_z;
   // gwp3[2] += pfile->table_offset_z;

   // calculate the direction cosines from the corners of the plane
   float read_dir[3];
   float phase_dir[3];
   float slice_dir[3];
   makeDirectionVectors(gwp1, gwp2, gwp3, read_dir, phase_dir, slice_dir);
   vecs->read_dir.x  = read_dir[0];
   vecs->read_dir.y  = read_dir[1];
   vecs->read_dir.z  = read_dir[2];
   vecs->phase_dir.x = phase_dir[0];
   vecs->phase_dir.y = phase_dir[1];
   vecs->phase_dir.z = phase_dir[2];
   vecs->slice_dir.x = slice_dir[0];
   vecs->slice_dir.y = slice_dir[1];
   vecs->slice_dir.z = slice_dir[2];
   vecs->center.x    = (gwp3[0] + gwp2[0]) / 2.0;
   vecs->center.y    = (gwp3[1] + gwp2[1]) / 2.0;
   vecs->center.z    = (gwp3[2] + gwp2[2]) / 2.0;

   return 0;
}



/**
 * Rotate based on patient position and convert to patient coordinate system
 * by swapping signs of X and Y
 *
 * @param entry 0="Head First", 1="Feet First"
 * @param pos 0="Supine", 1="Prone", 2="Decubitus Left", 3="Decubitus Right"
 * @param in original direction vector
 * @param out rotated direction vector
 * @returns 1 on success, -1 on failure
 */
int GenericConverter::rotateVectorOnPatient(unsigned int entry, unsigned int pos,
                                            float in[3], float out[3])
{
   if (entry > 1) {
      return -1;
   }
   if (pos > 3) {
      return -1;
   }

   float rot_hfs[3][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}};
   float rot_hfp[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
   float rot_hfdl[3][3] = {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}};
   float rot_hfdr[3][3] = {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}};
   float rot_ffs[3][3] = {{1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
   float rot_ffp[3][3] = {{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}};
   float rot_ffdl[3][3] = {{0, -1, 0}, {-1, 0, 0}, {0, 0, -1}};
   float rot_ffdr[3][3] = {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}};

   typedef float (*rot_mat_type)[3];
   rot_mat_type patient_rotations[2][4] = {
      { rot_hfs, rot_hfp, rot_hfdl, rot_hfdr },
      { rot_ffs, rot_ffp, rot_ffdl, rot_ffdr }
   };

   rot_mat_type rot = patient_rotations[entry][pos];

   out[0] = rot[0][0] * in[0] + rot[0][1] * in[1] + rot[0][2] * in[2];
   out[1] = rot[1][0] * in[0] + rot[1][1] * in[1] + rot[1][2] * in[2];
   out[2] = rot[2][0] * in[0] + rot[2][1] * in[1] + rot[2][2] * in[2];

   return 1;
}



/**
 * Calculates read, phase, and slice direction vectors from three corners of plane
 *
 * | r1 p1 s1 |   | x1 y1 z1 |
 * | r2 p2 s2 | = | x2 y2 z2 |
 * | r3 p3 s3 |   | x3 y3 z3 |
 *
 * @param
 * @param
 * @param
 * @param
 * @param
 * @param
 * @return
 */
void GenericConverter::makeDirectionVectors(float gwp1[3],     float gwp2[3],      float gwp3[3],
                                            float read_dir[3], float phase_dir[3], float slice_dir[3])
{
   /****Angulation of acquisition ****/
   /* Calculate rotation matrix */
   float x1 = gwp1[0], y1 = gwp1[1], z1 = gwp1[2];
   float x2 = gwp2[0], y2 = gwp2[1], z2 = gwp2[2];
   float x3 = gwp3[0], y3 = gwp3[1], z3 = gwp3[2];

   /* Calculate column 1 */
   float r1, r2, r3, xd;
   r1 = (x2 - x1);
   r2 = (y2 - y1);
   r3 = (z2 - z1);
   xd = sqrt(r1 * r1 + r2 * r2 + r3 * r3);

   /* Calculate column 2 */
   float p1, p2, p3, yd;
   p1 = (x3 - x1);
   p2 = (y3 - y1);
   p3 = (z3 - z1);
   yd = sqrt(p1 * p1 + p2 * p2 + p3 * p3);

   /* Calculate column 3, cross-product (column 1 x column 2) */
   float s1, s2, s3, zd;
   s1 = (r2 * p3) - (r3 * p2);
   s2 = (r3 * p1) - (r1 * p3);
   s3 = (r1 * p2) - (r2 * p1);
   zd = sqrt(s1 * s1 + s2 * s2 + s3 * s3);

   /* Fix cases where column length == 0 */
   if (xd == 0.0l) { r1 = 1.0l; r2 = r3 = 0.0l; xd = 1.0l; }
   if (yd == 0.0l) { p2 = 1.0l; p1 = p3 = 0.0l; yd = 1.0l; }
   if (zd == 0.0l) { s3 = 1.0l; s1 = s2 = 0.0l; zd = 1.0l; }

   /* Normalize columns */
   read_dir[0] =  r1 / xd; read_dir[1]  = r2 / xd; read_dir[2]  = r3 / xd;
   phase_dir[0] = p1 / yd; phase_dir[1] = p2 / yd; phase_dir[2] = p3 / yd;
   slice_dir[0] = s1 / zd; slice_dir[1] = s2 / zd; slice_dir[2] = s3 / zd;
}

} // namespace GEToIsmrmrd

