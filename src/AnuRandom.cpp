/*
 * implemented by Bartek 'BaSz' Szurgot (http://www.baszerr.eu)
 *
 */
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/Util/XMLConfiguration.h>
#include <cassert>
#include <sstream>

#include "AnuRandom.hpp"

using namespace std;
using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;

//
// helpers
//
namespace {

// reads html page from 'path' on host 'host' to stream 'ss'
void readHtml(const string& host, const string& path, stringstream& ss)
{
    try {
        HTTPClientSession s{ host };
        // send request
        HTTPRequest request{ HTTPRequest::HTTP_GET, path };
        s.sendRequest(request);
        // process response
        HTTPResponse response;
        istream& is = s.receiveResponse(response);
        if (response.getStatus() != HTTPResponse::HTTP_OK) // invalid status?
        {
            stringstream err;
            err << "invalid HTTP response from '" << host << "', while requesting '" << path << "' - status "
                << response.getStatus() << ": " << response.getReason();
            throw AnuRandom::ExceptionIO{ err.str() };
        }
        // copy stream's content to the output one
        Poco::StreamCopier::copyStream(is, ss);
    } catch (const AnuRandom::ExceptionIO& ex) {
        throw ex;
    } catch (const Poco::Exception& ex) {
        throw AnuRandom::ExceptionIO{ string{ "I/O error: " } + ex.displayText() };
    } catch (const std::exception& ex) {
        throw AnuRandom::ExceptionIO{ string{ "I/O error: " } + ex.what() };
    }
} // readHtml()

// XMLConfiguration has protected c-tor, so the wrapper class is required
struct HtmlReader : public XMLConfiguration {
    explicit HtmlReader(istream& is)
        : XMLConfiguration{ is }
    {
    }
};

// convert given char (hex) to numeric value
inline uint8_t toByte(const char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    const char lc = tolower(c);
    if ('a' <= lc && lc <= 'f')
        return lc - 'a' + 10;
    // oops - some error
    stringstream err;
    err << "non-hex passed for the convertion: " << c << " (" << int{ c } << ")";
    throw AnuRandom::ExceptionParseError{ err.str() };
} // toByte()

// convert given hex-string to byte array
void toByteArray(string str, AnuRandom::Data& out)
{
    // prepare space for the known-size array
    out.reserve(out.size() + str.length());

    // convert every two characters into a byte
    size_t i = 0;
    uint8_t byte = 0;
    for (char c : str) {
        // skip new lines, if present
        if (c == '\r' || c == '\n')
            continue;
        // conversion itself
        const uint8_t tmp = toByte(c);
        assert(tmp <= 0x0F);
        if ((i % 2) == 0)
            byte = tmp << 4;
        else {
            out.push_back(byte | tmp);
            byte = 0;
        }
        ++i;
    }
}

// parse input html in order to extract content - random data
void parseFromHtml(stringstream& html, AnuRandom::Data& out)
{
    const char* location = "body.div.div[1].table.tr.td"; // path to where random data is being stored
    try {
        HtmlReader xml{ html }; // reader
        const string str = xml.getString(location); // get given location's content
        toByteArray(move(str), out); // convert to output format
    } catch (const std::exception& ex) {
        throw AnuRandom::ExceptionParseError{ string{ "unable to parse '" } + location + "' from input: " + ex.what() };
    }
} // parseFromHtml()

} // unnamed namespace

//
// API's PIMPL implementation
//

/** \brief implementation class for AnuRandom.
 */
class AnuRandom::Impl {
public:
    /** \brief creates instance with a given parameters.
     *  \param host host to connect to.
     *  \param path path to download.
     */
    Impl(string&& host, string&& path)
        : host_{ move(host) }
        , path_{ move(path) }
    {
        if (path_.empty() || path_[0] != '/')
            throw ExceptionParameterError{ "invalid path '" + path_ + "' - must start with '/'" };
    }

    /** \brief read next portion of random data.
     *  \param out collection to output random bytes to.
     */
    void read(Data& out)
    {
        stringstream html;
        readHtml(host_, path_, html);
        parseFromHtml(html, out);
    }

private:
    const string host_; ///< host name or IP
    const string path_; ///< path to file on the server
};

//
// API implementation
//

AnuRandom::AnuRandom(std::string host, std::string path)
    : pimpl_{ new Impl{ move(host), move(path) } }
{
}

AnuRandom::~AnuRandom(void)
{
    // NOTE: this ensures proper destruction of implementation object
}

void AnuRandom::read(Data& out) { pimpl_->read(out); }
