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
#include <boost/spirit/include/qi.hpp>
#include <boost/range/as_literal.hpp>
#include "coroutine.hpp"

using namespace std;
using boost::asio::ip::tcp;

#include "yield.hpp"
class http_client
:   public boost::enable_shared_from_this<http_client>
,   private coroutine
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

    bool chunked_;
    unsigned int chunk_size_;

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
    ,   chunked_()
    ,   chunk_size_()
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

        auto this_ = shared_from_this();
        boost::asio::async_connect(socket_, endpoint_iterator,
            [this_](boost::system::error_code const& ec, tcp::resolver::iterator iterator) { return (*this_)(ec); });
    }

    void operator()(boost::system::error_code const& ec)
    {
        namespace qi = boost::spirit::qi;

        if (ec)
            return handle_error(ec);

        auto this_ = shared_from_this();
        auto call_self = [this_](boost::system::error_code const& ec, std::size_t bytes_transferred) {
            return (*this_)(ec);
        };
        reenter(this) {
            // Initialize
            chunked_ = false;
            cout << "Connected: http://" << host_;
            if (port_ != "80")
                cout << ":" << port_;
            cout << path_ << endl;

            // Send request
            yield {
                std::ostream request_stream(&request_);
                request_stream << "GET " << path_ << " HTTP/1.0\r\n"
                                  "Host: " << host_ << "\r\n"
                                  "Accept: */*\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                               ;
                boost::asio::async_write(socket_, request_, call_self);
            }

            // Receive status-line
            yield boost::asio::async_read_until(socket_, response_, "\r\n", call_self);
            {
                using boost::spirit::qi::ascii::digit;
                using boost::spirit::qi::ascii::space;

                // Parse chunk size
                const char *iter = boost::asio::buffer_cast<const char *>(response_.data());
                const char *last = strstr(iter, "\r\n");
                unsigned int consume_size = last - iter;
                auto r = qi::raw[ "HTTP/" >> digit >> "." >> digit ]
                         >> qi::raw[ qi::repeat(3)[ digit ] ]
                         >> qi::raw[ *(qi::char_ - "\r") ]
                         ;
                boost::iterator_range<const char *> http_version, status_code, message;
                qi::phrase_parse(iter, last, r, space, http_version, status_code, message);

                // Consume line
                response_.consume(consume_size);

                if (status_code != boost::as_literal("200")) {
                    std::cout << "Response returned with status code ";
                    std::cout << status_code << "\n";
                    return;
                }
            }

            // Receive header
            yield boost::asio::async_read_until(socket_, response_, "\r\n\r\n", call_self);
            {
                using boost::spirit::qi::ascii::space;
                const auto r = qi::lexeme[ *(qi::char_ - ":") ] >> ":" >> qi::lexeme[ *(qi::char_ - "\r") ];

                // Process the response headers.
                std::istream response_stream(&response_);
                std::string header;
                while (std::getline(response_stream, header) && header != "\r") {
                    std::cout << header << "\n";
                    string key, value;
                    if (qi::phrase_parse(header.begin(), header.end(), r, space, key, value)) {
                        if (key == "Transfer-Encoding" && value == "chunked")
                            chunked_ = true;
                    }
                }
                std::cout << "\n";
            }

            // Receive body
            if (chunked_) {
                while (1) {
                    yield boost::asio::async_read_until(socket_, response_, "\r\n", call_self);
                    {
                        // Parse chunk size
                        const char *iter = boost::asio::buffer_cast<const char *>(response_.data());
                        const char *last = strstr(iter, "\r\n");
                        unsigned int consume_size = last - iter;
                        qi::parse(iter, last, qi::hex, chunk_size_);

                        // Consume line
                        response_.consume(consume_size);
                    }

                    // read chunk body
                    if (chunk_size_ > 0) {
                        yield boost::asio::async_read(socket_, response_, boost::asio::transfer_at_least(chunk_size_ + 2), call_self);
                        {
                            const char *iter = boost::asio::buffer_cast<const char *>(response_.data());
                            const char *last = iter + chunk_size_;

                            if (strncmp(last, "\r\n", 2) != 0) {
                                // error
                            }

                            // do something
                            response_.consume(chunk_size_ + 2);
                        }
                    }

                    // last chunk
                    else {
                        // read trailer
                        while (1) {
                            yield boost::asio::async_read_until(socket_, response_, "\r\n", call_self);
                            {
                                const char *first = boost::asio::buffer_cast<const char *>(response_.data());
                                const char *last = strstr(first, "\r\n");

                                // Consume line
                                response_.consume(last + 2 - first);

                                if (first == last)
                                    goto chunk_finished;
                            }
                        }
                    }
                }
chunk_finished:
                ;
            }
            else {
                yield boost::asio::async_read(socket_, response_, call_self);
                // cout << boost::asio::buffer_cast<const char *>(response_.data()) << endl;
                // do something
                response_.consume(response_.size());
            }
        }
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
