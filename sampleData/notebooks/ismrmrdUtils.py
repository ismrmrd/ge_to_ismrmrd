
import   numpy          as       np
import   ismrmrd        as       mrd
import   ismrmrd.xsd
from     matplotlib     import   pyplot   as plt



class rawMRutils:



    # def __init__ (self):



   def returnHeaderAndData (h5RawFilePath, dataElement='/dataset'):
   
      '''
         Given an ISMRMRD data file, return a tuple with the first
         element being the XML header serialized into a structure,
         and the 2nd element a Numpy array with the indices ordered
         according to the following shape:
   
         [coil, readout, phase_encode_1, phase_encode_2 or slice, time or contrast]
      '''
   
      rawDataArray = mrd.Dataset(h5RawFilePath, dataElement, True)
      rawDataArrayHeader = ismrmrd.xsd.CreateFromDocument(rawDataArray.read_xml_header())
   
      enc = rawDataArrayHeader.encoding[0]
   
      # Matrix size
      eNx = enc.encodedSpace.matrixSize.x
      eNy = enc.encodedSpace.matrixSize.y
      # eNz = enc.encodedSpace.matrixSize.z
   
      if enc.encodingLimits.slice != None:
         eNz = enc.encodingLimits.slice.maximum + 1
      else:
         eNz = 1
   
      if enc.encodingLimits.repetition != None:
         eNt = enc.encodingLimits.repetition.maximum + 1
      else:
         eNt = 1
   
      # This will pack k-space data into a numpy array with the following
      # data order / shape:
      #
      #    [coil, readout, phase_encode_1, phase_encode_2 or slice, time]
      #
      # This ordering may not be suitable for all, or even a majority of
      # applications.  But since this is a demonstration notebook, it
      # seemed to be a 'natural' way to unpack and organize the data from
      # the ISMRMRD raw file.
   
      allKspace = np.zeros((rawDataArray.read_acquisition(0).data.shape[0],
                            rawDataArray.read_acquisition(0).data.shape[1],
                            eNy, eNz, eNt), dtype=np.complex64)
   
      for i in range(rawDataArray.number_of_acquisitions()):
         thisAcq = rawDataArray.read_acquisition(i)
         allKspace[:, :, thisAcq.idx.kspace_encode_step_1, thisAcq.idx.slice, thisAcq.idx.contrast] = thisAcq.data
   
      return rawDataArrayHeader, allKspace
   
   
   
   def computeAndPlot (arraySent2Plot, quant='magnitude', coil=-1, cmap='viridis'):
   
      # print ("shape of sent array is: ", arraySent2Plot.shape)
   
      if (coil == -1):
         array2Plot = arraySent2Plot
      else:
         array2Plot = np.squeeze(arraySent2Plot[coil, :, :, :, :])
   
      # print ("shape of plotted array is: ", array2Plot.shape)
   
      # from comment above, the next to last index is
      # the slice slot, while the last index is
      #  repetition / contrast.
      slices2Plot = array2Plot.shape[-2]
      times2Plot  = array2Plot.shape[-1]
   
      # print ("slices:   ", slices2Plot)
      # print ("time pts: ", times2Plot)
   
      imageCols = int(np.ceil(np.sqrt(slices2Plot)))
      imageRows = int(np.ceil(np.sqrt(slices2Plot))) * times2Plot
   
      plottedFigures = plt.figure(figsize=(12,18))
   
      for t in range(times2Plot):
         for s in range(slices2Plot):
            subImages = plottedFigures.add_subplot(imageRows, imageCols, ((t * imageRows * imageCols / times2Plot) + s + 1))
   
            if ((quant == 'angle') or (quant == 'phase')):
               if (coil == -1):
                  reconnedImage = np.sum(np.angle(array2Plot[:, :, :, s, t]), axis=0)
               else:
                  reconnedImage = (np.angle(array2Plot[:, :, s, t]))
            else:
               if (coil == -1):
                  reconnedImage = np.sqrt(np.sum((abs(array2Plot[:, :, :, s, t])), axis=0))
               else:
                  reconnedImage = np.sqrt(((abs(array2Plot[:, :, s, t]))))
   
            subImages.imshow(reconnedImage, cmap)

