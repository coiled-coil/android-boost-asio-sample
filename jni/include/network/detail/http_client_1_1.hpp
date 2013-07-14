#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/range/as_literal.hpp>
#include <network/http_1_1/async_operation.hpp>
#include <parse_header.h>
#include <boost/asio/coroutine.hpp>
#include <network/http_client.hpp>

namespace network {
namespace detail {

using namespace std;
using boost::asio::ip::tcp;

#include <boost/asio/yield.hpp>
template <
    typename ResponseHandler  // handler(boost::asio::error_code ec, const char *buf, size_t length)
>
class http_client_1_1
:   public http_client
,   public boost::enable_shared_from_this<http_client_1_1<ResponseHandler>>
,   private boost::asio::coroutine
{
private:
    typedef http_client_1_1 self_t;

private:
    ResponseHandler response_handler_;
    boost::asio::io_service& io_service_;
    std::string host_, port_, path_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::streambuf request_;
    boost::asio::streambuf response_;
    std::vector<char> buffer_;

    int status_code_;
    unsigned int chunk_size_;
    request_header_t header_;
    bool more_;

public:
    explicit http_client_1_1(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::string const& path, ResponseHandler&& response_handler)
    :   response_handler_(response_handler)
    ,   io_service_(io_service)
    ,   host_(host)
    ,   port_(port)
    ,   path_(path)
    ,   resolver_(io_service)
    ,   socket_(io_service)
    ,   request_()
    ,   response_()
    ,   buffer_()
    ,   status_code_()
    ,   chunk_size_()
    ,   header_()
    ,   more_()
    {
        header_.content_length = -1;
    }

    http_client_1_1(http_client_1_1&&) = default;
    http_client_1_1(http_client_1_1&) = delete;
    http_client_1_1() = delete;
    http_client_1_1& operator=(http_client_1_1 const&) = delete;
    http_client_1_1& operator=(http_client_1_1&&) = default;

    void start_request()
    {
        tcp::resolver::query query(host_, port_);
        resolver_.async_resolve(query,
            boost::bind(&self_t::handle_resolve, this->shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::iterator));
    }

    void handle_resolve(boost::system::error_code const& ec, tcp::resolver::iterator endpoint_iterator)
    {
        if (ec)
            return handle_error(ec);

        auto this_ = this->shared_from_this();
        boost::asio::async_connect(socket_, endpoint_iterator,
            [this_](boost::system::error_code const& ec, tcp::resolver::iterator iterator) { return (*this_)(ec); });
    }

    void operator()(boost::system::error_code const& ec)
    {
        if (ec)
            return handle_error(ec);

        auto this_ = this->shared_from_this();
        auto call_self = [this_](boost::system::error_code const& ec, std::size_t bytes_transferred) {
            return (*this_)(ec);
        };
        reenter(this) {
            // Initialize
            cout << "Connected: http://" << host_;
            if (port_ != "80")
                cout << ":" << port_;
            cout << path_ << endl;

            // Send request
            yield {
                std::ostream request_stream(&request_);
                request_stream << "GET " << path_ << " HTTP/1.1\r\n"
                                  "Host: " << host_ << "\r\n"
                                  "Accept: */*\r\n"
                                  "Connection: keep-alive\r\n"
                                  "\r\n"
                               ;
                boost::asio::async_write(socket_, request_, call_self);
            }

            // Receive status-line
            yield network::http_1_1::async_read_status_line(socket_, response_, status_code_, call_self);
cout << "STATUS_LINE: status_code=" << status_code_ << endl;

            // Receive header
            yield network::http_1_1::async_read_header(socket_, response_, header_, call_self);
cout << "HEADER: content_length=" << header_.content_length << " chunked=" << header_.chunked << " keep_alive=" << header_.keep_alive << endl;

            // Receive body
            if (header_.chunked) {
                buffer_.clear();
                while (1) {
                    chunk_size_ = 0;
                    yield network::http_1_1::async_read_chunk_size(socket_, response_, chunk_size_, call_self);
                    cout << "CHUNK SIZE = " << chunk_size_ << endl;

                    // read chunk body
                    if (chunk_size_ > 0) {
                        yield network::http_1_1::async_read_chunk_body(socket_, response_, chunk_size_, &buffer_, call_self);
                    }

                    // last chunk
                    else {
                        // read trailer
                        while (1) {
                            yield network::http_1_1::async_read_trailer(socket_, response_, more_, call_self);
                            if (! more_)
                                goto chunk_finished;
                        }
                    }
                }
chunk_finished:
                response_handler_(ec, buffer_);
                ;
            }
            else if (header_.content_length >= 0) {
                buffer_.clear();
                yield network::http_1_1::async_read_body(socket_, response_, header_.content_length, &buffer_, call_self);
                response_handler_(ec, buffer_);
            }
            else {
                buffer_.clear();
                yield network::http_1_1::async_read_body(socket_, response_, &buffer_, call_self);
                response_handler_(ec, buffer_);
            }
        }
    }

    void handle_error(boost::system::error_code const& ec)
    {
        response_handler_(ec, buffer_);
        cout << "Error: " << ec << endl;
    }
};
#include <boost/asio/unyield.hpp>

} // namespace detail
} // namespace network
