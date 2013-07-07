#pragma once

#include <parse_header.h>
#include <cstdlib>

namespace network {
namespace http_1_1 {

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_status_line(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    int& status_code,
    ReadHandler handler)
{
    auto callback = [&, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec) {
            const char *p = boost::asio::buffer_cast<const char *>(b.data());
            if (strncmp(p, "HTTP/1.", 7) == 0) {
                if (p[7] == '0' || p[7] == '1') {
                    if (p[8] == ' ') {
                        status_code = atoi(p + 9);
                    }
                }
            }
            b.consume(bytes_transferred);
        }

        handler(ec, bytes_transferred);
    };

    boost::asio::async_read_until(s, b, "\r\n", callback);
}

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_header(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    request_header_t &header,
    ReadHandler handler)
{
    auto callback = [&, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec) {
            const char *iter = boost::asio::buffer_cast<const char *>(b.data());
            parse_header(iter, bytes_transferred, &header);
            b.consume(bytes_transferred);
        }

        handler(ec, bytes_transferred);
    };

    boost::asio::async_read_until(s, b, "\r\n\r\n", callback);
}

/*
    rfc2616 $3.6.1 Chunked Transfer Coding

       Chunked-Body   = *chunk
                        last-chunk
                        trailer
                        CRLF
       chunk          = chunk-size [ chunk-extension ] CRLF
                        chunk-data CRLF
       chunk-size     = 1*HEX
       last-chunk     = 1*("0") [ chunk-extension ] CRLF
       chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
       chunk-ext-name = token
       chunk-ext-val  = token | quoted-string
       chunk-data     = chunk-size(OCTET)
       trailer        = *(entity-header CRLF)
*/

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_trailer(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    bool& more,
    ReadHandler handler)
{
    auto callback = [&, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec) {
            b.consume(bytes_transferred);
            more = bytes_transferred != 2;
        }

        handler(ec, bytes_transferred);
    };

    boost::asio::async_read_until(s, b, "\r\n", callback);
}

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_chunk_size(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    unsigned int& chunk_size,
    ReadHandler handler)
{
    using std::strtoul;

    auto callback = [&, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec) {
            // Parse chunk size
            const char *iter = boost::asio::buffer_cast<const char *>(b.data());
            char *last;
            chunk_size = strtoul(iter, &last, 16);

            // Consume line
            b.consume(bytes_transferred);
        }

        handler(ec, bytes_transferred);
    };

    boost::asio::async_read_until(s, b, "\r\n", callback);
}

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_chunk_body(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    unsigned int chunk_size,
    ReadHandler handler)
{
    auto callback = [&, chunk_size, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec)
            b.consume(chunk_size + 2);

        handler(ec, bytes_transferred);
    };

    if (b.size() < chunk_size + 2) {
        boost::asio::async_read(s, b, boost::asio::transfer_at_least(chunk_size + 2 - b.size()), callback);
    }
    else {
        callback(boost::system::error_code(), 0);
    }
}

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_body(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    unsigned int content_length,
    ReadHandler handler)
{
    auto callback = [&, content_length, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec)
            b.consume(content_length);

        handler(ec, bytes_transferred);
    };

    if (b.size() < content_length) {
        boost::asio::async_read(s, b, boost::asio::transfer_at_least(content_length - b.size()), callback);
    }
    else {
        callback(boost::system::error_code(), 0);
    }
}

template<
    typename AsyncReadStream,
    typename ReadHandler
>
void async_read_body(
    AsyncReadStream & s,
    boost::asio::streambuf& b,
    ReadHandler handler)
{
    auto callback = [&, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec)
            b.consume(b.size());

        handler(ec, bytes_transferred);
    };

    boost::asio::async_read(s, b, callback);
}

} // namespace http_1_1
} // namespace network
