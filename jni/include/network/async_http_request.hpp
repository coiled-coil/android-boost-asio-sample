#pragma once

#include <string>
#include <boost/asio.hpp>
#include <network/detail/http_client_1_1.hpp>

namespace network {

template <typename ResponseHandler>
inline
boost::shared_ptr<http_client>
async_http_request(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::string const& path, ResponseHandler&& handler)
{
    // Close connection when c's destructor is called.
    auto c = boost::make_shared<detail::http_client_1_1<ResponseHandler>>(io_service, host, port, path, std::move(handler));
    c->start_request();
    return c;
}

} // namespace network
