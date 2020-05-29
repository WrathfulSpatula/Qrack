/*
 * API for getting truly random data from ANU Quantum Random Number Server
 * (http://150.203.48.55/) in the byte-array form.
 *
 * implemented by Bartek 'BaSz' Szurgot (http://www.baszerr.eu)
 *
 */
#ifndef INCLUDE_ANURANDOM_HPP_FILE
#define INCLUDE_ANURANDOM_HPP_FILE

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/** \brief ANU random number random number generator server's API.
 */
class AnuRandom {
public:
    /** \brief exception thrown on parameter error.
     */
    struct ExceptionParameterError : public std::runtime_error {
        /** \brief creates exception with a given message.
         *  \param msg message to be shown.
         */
        explicit ExceptionParameterError(std::string msg)
            : std::runtime_error{ std::move(msg) }
        {
        }
    };

    /** \brief exception thrown on I/O error (communication, sockets, etc...)
     */
    struct ExceptionIO : public std::runtime_error {
        /** \brief creates exception with a given message
         *  \param msg message to be shown.
         */
        explicit ExceptionIO(std::string msg)
            : std::runtime_error{ std::move(msg) }
        {
        }
    };

    /** \brief exception thrown on input parse error.
     */
    struct ExceptionParseError : public std::runtime_error {
        /** \brief creates exception with a given message.
         *  \param msg message to be shown.
         */
        explicit ExceptionParseError(std::string msg)
            : std::runtime_error{ std::move(msg) }
        {
        }
    };

    /** \brief output type, holding the read data. */
    typedef std::vector<uint8_t> Data;

    /** \brief create object for reading random data.
     *
     *  c-tor does not connect to the server. connections are established
     *  on demand, when reads are requested. each time new connection is opened.
     *
     *  default arguments are fine for server settings as for writing this code.
     *  stick to defaults, since they can change in the future.
     *
     *  \param host to connect to.
     *  \param path path on the host, where raw hex data can be taken from.
     */
    explicit AnuRandom(std::string host = "150.203.48.55", std::string path = "/RawHex.php");
    /** \brief deallocator for the pointer-to-implementation.
     */
    ~AnuRandom(void);

    /** \brief reads random data from the server.
     *  \param out output array to append random data to.
     */
    void read(Data& out);

    /** \brief reads random data from the server.
     *  \return new portion of random data.
     */
    Data read(void)
    {
        Data tmp;
        read(tmp);
        return tmp;
    }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif
