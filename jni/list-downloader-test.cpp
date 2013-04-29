#include <string>
#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include "coroutine.hpp"

using namespace std;
using boost::asio::ip::tcp;

#include "yield.hpp"
class http_client
:   public boost::enable_shared_from_this<http_client>
{
private:
    typedef http_client self_t;

private:
    boost::asio::io_service& io_service_;
    std::string host_, port_, path_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::streambuf request_;
    boost::asio::streambuf response_;

public:
    explicit http_client(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::string const& path)
    :   io_service_(io_service)
    ,   host_(host)
    ,   port_(port)
    ,   path_(path)
    ,   resolver_(io_service)
    ,   socket_(io_service)
    ,   request_()
    ,   response_()
    {
    }

    http_client(http_client&&) = default;
    http_client(http_client&) = delete;
    http_client() = delete;
    http_client& operator=(http_client const&) = delete;
    http_client& operator=(http_client&&) = default;

    void start_request()
    {
        tcp::resolver::query query(host_, port_);
        resolver_.async_resolve(query,
            boost::bind(&self_t::handle_resolve, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::iterator));
    }

    void handle_resolve(boost::system::error_code const& ec, tcp::resolver::iterator endpoint_iterator)
    {
        if (ec)
            return handle_error(ec);

        boost::asio::async_connect(socket_, endpoint_iterator,
            boost::bind(&self_t::handle_connect, shared_from_this(),
                boost::asio::placeholders::error));
    }

    void handle_connect(boost::system::error_code const& ec)
    {
        if (ec)
            return handle_error(ec);

        cout << "Connected: http://" << host_;
        if (port_ != "80")
            cout << ":" << port_;
        cout << path_ << endl;
    }

    void handle_error(boost::system::error_code const& ec)
    {
        cerr << "Error: " << ec << endl;
    }
};
#include "unyield.hpp"

inline
boost::shared_ptr<http_client>
async_request(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::string const& path)
{
    // Close connection when c's destructor is called.
    auto c = boost::make_shared<http_client>(io_service, host, port, path);
    c->start_request();
    return c;
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        cerr << "Invalid arguments." << endl;
        return 1;
    }

    boost::asio::io_service io_service;

    async_request(io_service, argv[1], argv[2], argv[3]);

    io_service.run();

    return 0;
}
