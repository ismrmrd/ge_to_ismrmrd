
/** @file GenericConverter.cpp */
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "GenericConverter.h"

namespace PfileToIsmrmrd {

int GenericConverter::get_view_idx(GERecon::Legacy::Pfile *pfile,
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

    const GERecon::Control::ProcessingControlPointer processingControl(pfile->CreateOrchestraProcessingControl());
    unsigned int nframes = processingControl->Value<int>("AcquiredYRes");

    // Check the pfile header for the data order
    // OLD: if (pfile->dacq_ctrl & PF_RAW_COLLECT) {
    if (pfile->IsRawMode()) {
        // RDB_RAW_COLLECT bit is set, so data is in view order on disk
        // Acquisition looping order is sequence dependent, as has to be
        // implemented per sequence.
        idx.repetition = view_num / (pfile->SliceCount() * nframes);
        view_num = view_num % (pfile->SliceCount() * nframes);

        idx.kspace_encode_step_1 = view_num / pfile->SliceCount();
        view_num = view_num % pfile->SliceCount();

        idx.slice = view_num;
    }
    else {
        // RDB_RAW_COLLECT bit is NOT set, so data is in default ge order on disk
        // Default looping order:
        // repetitionloop (nreps)
        //   sliceloop (SliceCount())
        //     mean baseline (1)
        //       kyloop (nframes)
        idx.repetition = view_num / (pfile->SliceCount() * (1 + nframes));
        // view_num = view_num % (pfile->SliceCount() * (1 + nframes));

        // idx.slice = view_num / (1 + nframes);
        // view_num = view_num % (1 + nframes);

        // put the frame number in kspace_encode_step_1
        if (view_num < 1) {
            // this is the mean baseline view return -1
            return -1;
        }
        // this is a regular line (frame)
        // idx.kspace_encode_step_1 = view_num - 1;
    }

    return 1;
}



std::vector<ISMRMRD::Acquisition> GenericConverter::getAcquisitions(
        GERecon::Legacy::Pfile* pfile, unsigned int acq_mode)
{
    std::vector<ISMRMRD::Acquisition> acqs;

    const GERecon::Control::ProcessingControlPointer processingControl(pfile->CreateOrchestraProcessingControl());
    unsigned int nPhases = processingControl->Value<int>("AcquiredYRes");
    unsigned int nEchoes = processingControl->Value<int>("NumEchoes");
    unsigned int nChannels = pfile->ChannelCount();

    // Make number of acquisitions to be converted
    acqs.resize(pfile->AcquiredSlicesPerAcq() * nEchoes * nPhases);

    unsigned int acq_num = 0;

    // Orchestra API provides size in bytes.
    // frame_size is the number of complex points in a single channel
    // size_t frame_size = pfile->ViewSize() / pfile->SampleSize();
    size_t frame_size = processingControl->Value<int>("AcquiredXRes");

    for (int sliceCount = 0 ; sliceCount < pfile->AcquiredSlicesPerAcq() ; sliceCount++)
    {
        for (int echoCount = 0 ; echoCount < nEchoes ; echoCount++)
        {
            for (int phaseCount = 0 ; phaseCount < nPhases ; phaseCount++)
            {
                // std::cout << "Processing acquisition " << acq_num
                // << " phase count " << phaseCount << " slice count " << sliceCount << std::endl;

                // Grab a reference to the acquisition
                ISMRMRD::Acquisition& acq = acqs.at(acq_num);

                // Set size of this data frame to receive raw data
                acq.resize(frame_size, nChannels, 0);

                // Get data from P-file using KSpaceData object, and copy
                // into ISMRMRD space.
                for (int channelCount = 0 ; channelCount < nChannels ; channelCount++)
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

                    auto kData = pfile->KSpaceData<float>(sliceCount, echoCount,
                                     channelCount);

                    // Need to check that this indexing of kData is correct, and it's not kData(phaseCount, i)
                    for (int i = 0 ; i < frame_size ; i++)
                       acq.data(i, channelCount) = kData(i, phaseCount);
                }

                // Initialize the encoding counters for this acquisition.
                ISMRMRD::EncodingCounters idx;
                get_view_idx(pfile, 0, idx);

                idx.slice = sliceCount;
                idx.contrast  = echoCount;
                idx.kspace_encode_step_1 = phaseCount;

                acq.idx() = idx;

                // Fill in the rest of the header
                acq.clearAllFlags();
                acq.measurement_uid() = pfile->RunNumber();
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

                acq_num++;
            } // end of phaseCount loop
        } // end of echoCount loop
    } // end of sliceCount loop

    return acqs;
}

SEQUENCE_CONVERTER_FACTORY_DECLARE(GenericConverter)

} // namespace PfileToIsmrmrd

