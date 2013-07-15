#pragma once

#include <boost/system/error_code.hpp>

namespace network {

class http_client;

using request_handler_t = boost::function< void (http_client *client, std::ostream& request_stream, std::string const& host, std::string const& port) >;
using response_handler_t = boost::function< void (http_client *client, boost::system::error_code ec, std::vector<char>& buf) >;

class http_client
{
public:
    virtual ~http_client() = 0;
    virtual void start(request_handler_t&& request_handler, response_handler_t&& response_handler) = 0;
    virtual void cancel() = 0;
};

inline
http_client::~http_client()
{
}

} // namespace network
