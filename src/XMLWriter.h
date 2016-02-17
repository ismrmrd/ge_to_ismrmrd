#pragma once
#ifndef XMLWriter_h
#define XMLWriter_h

extern "C" {
#include <libxml/xmlwriter.h>
}

#include <string>
#include <stdexcept>

class XMLWriter {
public:
    XMLWriter() {
        int rc = 0;
        xmlTextWriterPtr writer;
        xmlBufferPtr buf;

        /* initialize libxml and check for a version mismatch */
        LIBXML_TEST_VERSION;

        /* create the XML buffer, to which the XML doc will be written */
        buf = xmlBufferCreate();
        if (buf == NULL) {
            throw std::runtime_error("Error creating xml buffer");
        }

        /* create new XmlWriter with no compression */
        writer = xmlNewTextWriterMemory(buf, 0);
        if (writer == NULL) {
            throw std::runtime_error("Error creating the xml writer");
        }

        /* turn on auto indentation (which is not guaranteed to work) */
        rc = xmlTextWriterSetIndent(writer, 1);
        if (rc < 0) {
            throw std::runtime_error("Error enabling xml indentation");
        }
        rc = xmlTextWriterSetIndentString(writer, BAD_CAST "    ");
        if (rc < 0) {
            throw std::runtime_error("Error enabling xml indent string");
        }

        writer_ = writer;
        buffer_ = buf;
    }

    ~XMLWriter() {
        xmlFreeTextWriter(writer_);
        xmlBufferFree(buffer_);
        xmlCleanupParser();
    }

    void startDocument() {
        /* start doc. version=1.0, encoding=default, standalone=default */
        int rc = xmlTextWriterStartDocument(writer_, NULL, NULL, NULL);
        if (rc < 0) {
            throw std::runtime_error("Error starting xml document");
        }
    }

    void endDocument() {
        int rc = xmlTextWriterEndDocument(writer_);
        if (rc < 0) {
            throw std::runtime_error("Error ending xml document");
        }
    }

    void startElement(const std::string& name) {
        int rc = xmlTextWriterStartElement(writer_, BAD_CAST name.c_str());
        if (rc < 0) {
            throw std::runtime_error("Error starting xml element" + name);
        }
    }

    void endElement() {
        int rc = xmlTextWriterEndElement(writer_);
        if (rc < 0) {
            throw std::runtime_error("Error ending xml element");
        }
    }

    template <typename... Args>
    void formatElement(const std::string& name, const std::string& format, Args... args)
    {
        int rc = xmlTextWriterWriteFormatElement(writer_, BAD_CAST name.c_str(), format.c_str(), args...);
        if (rc < 0) {
            throw std::runtime_error("Error formatting xml element");
        }
    }

    void addBooleanElement(const std::string& name, bool value) {
        formatElement(name, "%s", value ? "true" : "false");
    }

    std::string getXML() {
        return std::string((char *) xmlBufferContent(buffer_));
    }

    //xmlTextWriterPtr getWriter(void) { return writer_; }
    //xmlBufferPtr getBuffer(void) { return buffer_; }

private:
    xmlTextWriterPtr writer_;
    xmlBufferPtr buffer_;
};

#endif  // XMLWriter_h
