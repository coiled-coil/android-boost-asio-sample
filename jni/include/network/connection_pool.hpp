#pragma once

#include <vector>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <network/http_client.hpp>
#include <boost/asio/coroutine.hpp>

namespace network {

using http_job_t = std::pair<request_handler_t, response_handler_t>;
class connection_pool;

#include <boost/asio/yield.hpp>
class task_t : private boost::asio::coroutine
{
public:
    explicit task_t(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::vector<http_job_t>& request_queue)
    :   io_service_(io_service)
    ,   host_(host)
    ,   port_(port)
    ,   request_queue_(request_queue)
    ,   client_(make_http_client(io_service, host, port))
    {
        std::vector<char> dummy;
        (*this)(&client_, boost::system::error_code(), dummy);
    }

    void operator()(http_client *client, boost::system::error_code ec, std::vector<char>& buf)
    {
        reenter(this) {

            while ( !request_queue_.empty()) {
                http_job_t job = request_queue_.back();
                request_handler_t& request_handler = job.first;
                response_handler_t& response_handler = job.second;


                yield client->start(request_handler, *this);
                response_handler();
                request_queue_.pop_back();
            }
        }
    }

private:
    boost::asio::io_service& io_service_;
    std::string const& host_;
    std::string const& port_;
    std::vector<http_job_t>& request_queue_;
    boost::shared_ptr<http_client> client_;
};
#include <boost/asio/unyield.hpp>

class connection_pool
{
public:
    explicit connection_pool(boost::asio::io_service& io_service, std::string const& host, std::string const& port, std::size_t n)
    :   io_service_(io_service)
    ,   host_(host)
    ,   port_(port)
    ,   pool_(n)
    ,   request_queue_()
    {
    }

    void add(request_handler_t&& request_handler, response_handler_t&& response_handler)
    {
        for (auto& p : pool_) {
            if (p.expired()) {
                p = async_http_request(io_service_, host_, port_, std::move(request_handler),
                    [this, response_handler](http_client *client, boost::system::error_code ec, std::vector<char>& buf) {
                        response_handler(client, ec, buf);

                        if (! request_queue_.empty()) {
                            http_job_t job = request_queue_.back();
                            client->start(std::move(job.first), std::move(job.second));
                            request_queue_.pop_back();
                        }
                    }
                );
                return;
            }
        }

        request_queue_.push_back(std::make_pair(request_handler, response_handler));
    }

#if 0
    template <typename HttpClient, typename... Args>
    boost::shared_ptr<http_client>
    try_add(Args...&& args)
    {
        for (auto& p : pool_) {
            if (!p || !p.lock()) {
                auto client = boost::make_shared<HttpClient>(args...);
                p = client;
                return client;
            }
        }
        return {};
    }
#endif

private:
    boost::asio::io_service& io_service_;
    std::string host_;
    std::string port_;
    std::vector<boost::weak_ptr<http_client>> pool_;
    std::vector<http_job_t> request_queue_;
};

} // namespace network
