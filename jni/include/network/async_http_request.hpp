#pragma once

#include <string>
#include <boost/asio.hpp>
#include <network/detail/http_client_1_1.hpp>

namespace network {

inline
boost::shared_ptr<http_client>
make_http_client(boost::asio::io_service& io_service, std::string const& host, std::string const& port)
{
    // Close connection when c's destructor is called.
    return boost::make_shared<detail::http_client_1_1>(io_service, host, port);
}

inline
boost::shared_ptr<http_client>
async_http_request(boost::asio::io_service& io_service, std::string const& host, std::string const& port, request_handler_t&& request_handler, response_handler_t&& handler)
{
    auto client = make_http_client(io_service, host, port);
    client->start(std::move(request_handler), std::move(handler));
    return client;
}

inline
boost::shared_ptr<http_client>
async_http_get(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::string const& path, response_handler_t&& handler)
{
    auto http_get_request_handler = [path](http_client *, std::ostream& request_stream, std::string const& host, std::string const& port) {
        request_stream << "GET " << path << " HTTP/1.1\r\n"
                          "Host: " << host << "\r\n"
                          "Accept: */*\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n"
                       ;
    };

    auto client = make_http_client(io_service, host, port);
    client->start(std::move(http_get_request_handler), std::move(handler));
    return client;
}

} // namespace network
