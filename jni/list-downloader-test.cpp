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
#include <boost/spirit/include/qi.hpp>
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
                // Check that response is OK.
                std::istream response_stream(&response_);
                std::string http_version;
                response_stream >> http_version;
                unsigned int status_code;
                response_stream >> status_code;
                std::string status_message;
                std::getline(response_stream, status_message);
                if (!response_stream || http_version.substr(0, 5) != "HTTP/")
                {
                    std::cout << "Invalid response\n";
                    return;
                }
                if (status_code != 200)
                {
                    std::cout << "Response returned with status code ";
                    std::cout << status_code << "\n";
                    return;
                }
            }

            // Receive header
            yield boost::asio::async_read_until(socket_, response_, "\r\n\r\n", call_self);
            {
                namespace qi = boost::spirit::qi;
                using boost::spirit::lexeme;
                const auto r = lexeme[*(qi::char_ - ":")] >> ":" >> lexeme[*(qi::char_ - "\r")];

                // Process the response headers.
                std::istream response_stream(&response_);
                std::string header;
                while (std::getline(response_stream, header) && header != "\r") {
                    std::cout << header << "\n";
                    string key, value;
                    if (qi::phrase_parse(header.begin(), header.end(), r, qi::space, key, value)) {
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
                        std::istream response_stream(&response_);
                        std::string line;
                        chunk_size_ = 0;
                        response_stream >> hex >> chunk_size_;
                        getline(response_stream, line);
                    }

                    // read chunk body
                    if (chunk_size_ > 0) {
                        yield boost::asio::async_read(socket_, response_, boost::asio::transfer_at_least(chunk_size_), call_self);
                        // do something
                        response_.consume(chunk_size_);

                        // read chunk separator
                        yield boost::asio::async_read_until(socket_, response_, "\r\n", call_self);
                        {
                            std::istream response_stream(&response_);
                            std::string line;
                            getline(response_stream, line);
                        }
                    }

                    // last chunk
                    else {
                        // read trailer
                        while (1) {
                            yield boost::asio::async_read_until(socket_, response_, "\r\n", call_self);
                            {
                                std::istream response_stream(&response_);
                                std::string line;
                                getline(response_stream, line);

                                if (line == "\r")
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
