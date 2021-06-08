<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="xml" indent="yes"/>

  <xsl:template match="/">
    <ismrmrdHeader xsi:schemaLocation="http://www.ismrm.org/ISMRMRD ismrmrd.xsd"
      xmlns="http://www.ismrm.org/ISMRMRD"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xmlns:xs="http://www.w3.org/2001/XMLSchema">

      <subjectInformation>
        <patientName><xsl:value-of select="Header/Patient/Name"/></patientName>
        <patientWeight_kg><xsl:value-of select="Header/Patient/Weight"/></patientWeight_kg>
        <patientID><xsl:value-of select="Header/Patient/ID"/></patientID>
        <xsl:if test="Header/Patient/Birthdate != ''">
          <xsl:variable name="patientDOB" select="Header/Patient/Birthdate"/>
          <patientBirthdate><xsl:value-of select="concat(substring($patientDOB,1,4),'-',
                                                         substring($patientDOB,5,2),'-',
                                                         substring($patientDOB,7,2))"/></patientBirthdate>
        </xsl:if>
        <xsl:if test="Header/Patient/Gender != ''">
          <patientGender><xsl:value-of select="Header/Patient/Gender"/></patientGender>
        </xsl:if>
      </subjectInformation>

      <studyInformation>
        <xsl:variable name="studyDate" select="Header/Study/Date"/>
        <studyDate><xsl:value-of select="concat(substring($studyDate,1,4),'-',
                                                substring($studyDate,5,2),'-',
                                                substring($studyDate,7,2))"/></studyDate>
        <xsl:variable name="studyTime" select="Header/Study/Time"/>
        <studyTime><xsl:value-of select="concat(substring($studyTime,1,2),':',
                                                substring($studyTime,3,2),':',
                                                substring($studyTime,5,2))"/></studyTime>
        <studyID><xsl:value-of select="Header/Study/Number"/></studyID>
        <xsl:choose>
          <xsl:when test="Header/Study/AccessionNumber != ''">
            <accessionNumber><xsl:value-of select="Header/Study/AccessionNumber"/></accessionNumber>
          </xsl:when>
          <xsl:otherwise>
            <accessionNumber><xsl:value-of select="0"/></accessionNumber>
          </xsl:otherwise>
        </xsl:choose>
        <xsl:if test="Header/Study/ReferringPhysician != ''">
          <referringPhysicianName><xsl:value-of select="Header/Study/ReferringPhysician"/></referringPhysicianName>
        </xsl:if>
        <xsl:if test="Header/Study/Description != ''">
          <studyDescription><xsl:value-of select="Header/Study/Description"/></studyDescription>
        </xsl:if>
        <studyInstanceUID><xsl:value-of select="Header/Study/UID"/></studyInstanceUID>
      </studyInformation>

      <measurementInformation>
        <measurementID><xsl:value-of select="Header/Series/Number"/></measurementID>
        <xsl:variable name="seriesDate" select="Header/Series/Date"/>
        <seriesDate><xsl:value-of select="concat(substring($seriesDate,1,4),'-',
                                                 substring($seriesDate,5,2),'-',
                                                 substring($seriesDate,7,2))"/></seriesDate>
        <xsl:variable name="seriesTime" select="Header/Series/Time"/>
        <seriesTime><xsl:value-of select="concat(substring($seriesTime,1,2),':',
                                                 substring($seriesTime,3,2),':',
                                                 substring($seriesTime,5,2))"/></seriesTime>
        <xsl:choose>
          <xsl:when test="Header/PatientPosition = '0'">
            <patientPosition>HFP</patientPosition>
          </xsl:when>
          <xsl:otherwise>
            <patientPosition>HFS</patientPosition>
          </xsl:otherwise>
        </xsl:choose>
        <initialSeriesNumber><xsl:value-of select="Header/Series/Number"/></initialSeriesNumber>
        <protocolName><xsl:value-of select="Header/Series/ProtocolName"/></protocolName>
        <seriesDescription><xsl:value-of select="Header/Series/Description"/></seriesDescription>
        <seriesInstanceUIDRoot><xsl:value-of select="Header/Series/UID"/></seriesInstanceUIDRoot>
        <!-- <frameOfReferenceUID><xsl:value-of select="Header/FrameOfReferenceUID"/></frameOfReferenceUID> -->
        <referencedImageSequence>
          <xsl:for-each select="Header/ReferencedImageUIDs">
            <xsl:if test=". != ''">
                <referencedSOPInstanceUID><xsl:value-of select="."/></referencedSOPInstanceUID>
            </xsl:if>
          </xsl:for-each>
        </referencedImageSequence>
      </measurementInformation>

      <acquisitionSystemInformation>
          <systemVendor><xsl:value-of select="Header/Equipment/Manufacturer"/></systemVendor>
          <systemModel><xsl:value-of select="Header/Equipment/ManufacturerModel"/></systemModel>
          <systemFieldStrength_T><xsl:value-of select="Header/Image/MagneticFieldStrength"/></systemFieldStrength_T>
        <relativeReceiverNoiseBandwidth>1.0</relativeReceiverNoiseBandwidth>
        <receiverChannels><xsl:value-of select="Header/ChannelCount"/></receiverChannels>
        <institutionName><xsl:value-of select="Header/Equipment/Institution"/></institutionName>
        <stationName><xsl:value-of select="Header/Equipment/Station"/></stationName>
        <deviceID><xsl:value-of select="Header/Equipment/DeviceSerialNumber"/></deviceID>
      </acquisitionSystemInformation>

      <experimentalConditions>
          <H1resonanceFrequency_Hz><xsl:value-of select="Header/Image/ImagingFrequency * 1000000"/></H1resonanceFrequency_Hz>
      </experimentalConditions>

      <encoding>
        <trajectory>cartesian</trajectory>
        <encodedSpace>
          <matrixSize>
            <x><xsl:value-of select="Header/AcquiredXRes"/></x>
            <y><xsl:value-of select="Header/AcquiredYRes"/></y>
            <xsl:choose>
                <xsl:when test="(Header/Is3DAcquisition)='true'">
                   <z><xsl:value-of select="Header/AcquiredZRes"/></z>
                </xsl:when>
                <xsl:otherwise>
                   <z><xsl:value-of select="1"/></z>
                </xsl:otherwise>
            </xsl:choose>
          </matrixSize>
          <fieldOfView_mm>
            <x><xsl:value-of select="Header/TransformXRes * Header/Image/PixelSizeX"/></x>
            <y><xsl:value-of select="Header/TransformYRes * Header/Image/PixelSizeY"/></y>
            <!-- <z><xsl:value-of select="Header/Image/SliceThickness + Header/Image/SliceSpacing"/></z> -->
            <z><xsl:value-of select="Header/Image/SliceThickness"/></z>
          </fieldOfView_mm>
        </encodedSpace>
        <reconSpace>
          <matrixSize>
            <x><xsl:value-of select="Header/TransformXRes"/></x>
            <y><xsl:value-of select="Header/TransformYRes"/></y>
            <xsl:choose>
                <xsl:when test="(Header/Is3DAcquisition)='true'">
                   <z><xsl:value-of select="Header/TransformZRes"/></z>
                </xsl:when>
                <xsl:otherwise>
                   <z><xsl:value-of select="1"/></z>
                </xsl:otherwise>
            </xsl:choose>
          </matrixSize>
          <fieldOfView_mm>
            <x><xsl:value-of select="Header/TransformXRes * Header/Image/PixelSizeX"/></x>
            <y><xsl:value-of select="Header/TransformYRes * Header/Image/PixelSizeY"/></y>
            <!-- <z><xsl:value-of select="Header/Image/SliceThickness + Header/Image/SliceSpacing"/></z> -->
            <z><xsl:value-of select="Header/Image/SliceSpacing"/></z>
          </fieldOfView_mm>
        </reconSpace>
        <encodingLimits>
          <kspace_encoding_step_1>
            <minimum>0</minimum>
            <maximum><xsl:value-of select="Header/AcquiredYRes - 1"/></maximum>
            <center><xsl:value-of select="floor(Header/AcquiredYRes div 2)"/> </center>
          </kspace_encoding_step_1>
          <kspace_encoding_step_2>
            <minimum>0</minimum>
            <maximum>0</maximum>
            <center>0</center>
          </kspace_encoding_step_2>
          <slice>
            <minimum>0</minimum>
            <maximum><xsl:value-of select="Header/SliceCount - 1"/></maximum>
            <center><xsl:value-of select="floor(Header/SliceCount div 2)"/></center>
          </slice>
          <set>
            <minimum>0</minimum>
            <maximum>0</maximum>
            <center>0</center>
          </set>
          <phase>
            <minimum>0</minimum>
            <maximum>0</maximum>
            <center>0</center>
          </phase>
          <repetition>
            <minimum>0</minimum>
            <xsl:choose>
              <xsl:when test="Header/RepetitionCount != ''">
                <maximum><xsl:value-of select="Header/RepetitionCount - 1"/></maximum>
                <center><xsl:value-of select="floor(Header/RepetitionCount div 2)"/></center>
              </xsl:when>
              <xsl:otherwise>
                <maximum><xsl:value-of select="1"/></maximum>
                <center><xsl:value-of select="0"/></center>
              </xsl:otherwise>
            </xsl:choose>
          </repetition>
          <segment>
            <minimum>0</minimum>
            <maximum>0</maximum>
            <center>0</center>
          </segment>
          <contrast>
            <minimum>0</minimum>
            <xsl:choose>
              <xsl:when test="Header/EchoCount != ''">
                <maximum><xsl:value-of select="Header/EchoCount - 1"/></maximum>
                <center><xsl:value-of select="floor(Header/EchoCount div 2)"/></center>
              </xsl:when>
              <xsl:otherwise>
                <maximum><xsl:value-of select="1"/></maximum>
                <center><xsl:value-of select="0"/></center>
              </xsl:otherwise>
            </xsl:choose>
          </contrast>
          <average>
            <minimum>0</minimum>
            <maximum>0</maximum>
            <center>0</center>
          </average>
        </encodingLimits>
        <echoTrainLength><xsl:value-of select="Header/Image/EchoTrainLength"/></echoTrainLength>
      </encoding>

      <sequenceParameters>
        <TR><xsl:value-of select="Header/Image/RepetitionTime div 1000"/></TR>
        <TE><xsl:value-of select="Header/Image/EchoTime div 1000"/></TE>
        <TE><xsl:value-of select="Header/Image/SecondEcho div 1000"/></TE>
        <TI><xsl:value-of select="Header/Image/InversionTime div 1000"/></TI>
        <flipAngle_deg><xsl:value-of select="Header/Image/FlipAngle"/></flipAngle_deg>
      </sequenceParameters>

      <userParameters>
        <userParameterString>
          <name>imageType</name>
          <value><xsl:value-of select="Header/Image/ImageType"/></value>
        </userParameterString>
        <userParameterString>
          <name>scanningSequence</name>
          <value><xsl:value-of select="Header/Image/ScanSequence"/></value>
        </userParameterString>
        <userParameterString>
          <name>sequenceVariant</name>
          <value><xsl:value-of select="Header/Image/SequenceVariant"/></value>
        </userParameterString>
        <userParameterString>
          <name>scanOptions</name>
          <value><xsl:value-of select="Header/Image/ScanOptions"/></value>
        </userParameterString>
        <userParameterString>
          <name>mrAcquisitionType</name>
          <!-- value><xsl:value-of select="Header/Image/AcquisitionType"/></value -->
          <xsl:choose>
             <xsl:when test="Header/Image/AcquisitionType = '2025'">
                <value>2D</value>
             </xsl:when>

             <xsl:when test="Header/Image/AcquisitionType = '2026'">
                <value>3D</value>
             </xsl:when>

             <xsl:otherwise>
                <value>Unknown</value>
             </xsl:otherwise>
          </xsl:choose>
        </userParameterString>
        <userParameterString>
          <name>triggerTime</name>
          <value><xsl:value-of select="Header/Image/TriggerTime"/></value>
        </userParameterString>
        <userParameterString>
           <name>freqEncodingDirection</name>
           <!--
              Orchestra now gives Phase Encode direction.  So when phase encode
is row, freq encode is column, and vice-versa.

               Row = 1025,
               Column = 1026,
               UnknownPhaseEncodeDirection = 8328 -->
          <xsl:choose>
             <xsl:when test="Header/Image/PhaseEncodeDirection = '1025'">
                <value>COL</value>
             </xsl:when>

             <xsl:when test="Header/Image/PhaseEncodeDirection = '1026'">
                <value>ROW</value>
             </xsl:when>

             <xsl:otherwise>
                <value>Unknown</value>
             </xsl:otherwise>
          </xsl:choose>
        </userParameterString>
      </userParameters>

    </ismrmrdHeader>
  </xsl:template>

</xsl:stylesheet>
