#pragma once

#include <vector>
#include <tuple>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <network/http_client.hpp>
#include <boost/asio/coroutine.hpp>

namespace network {

struct http_job_t
{
    request_handler_t request_handler;
    response_handler_t response_handler;

    http_job_t(request_handler_t const& request_handler0, response_handler_t const& response_handler0)
    :   request_handler(request_handler0)
    ,   response_handler(response_handler0)
    {}

    http_job_t(request_handler_t&& request_handler0, response_handler_t&& response_handler0)
    :   request_handler(std::move(request_handler0))
    ,   response_handler(std::move(response_handler0))
    {}

    http_job_t() = default;
    http_job_t(http_job_t const&) = default;
    http_job_t(http_job_t&&) = default;
    http_job_t& operator=(http_job_t const&) = default;
    http_job_t& operator=(http_job_t&&) = default;

#if 0
    void swap(http_job_t& other)
    {
        request_handler.swap(other.request_handler);
        response_handler.swap(other.response_handler);
    }
#endif
};

class connection_pool;

#include <boost/asio/yield.hpp>
class task_t : private boost::asio::coroutine
{
public:
    explicit task_t(boost::asio::io_service& io_service, std::vector<http_job_t>& request_queue, boost::shared_ptr<http_client> client)
    :   io_service_(io_service)
    ,   request_queue_(request_queue)
    ,   client_(client)
    {
    }

    task_t(task_t&&) = default;
    task_t(task_t const&) = default;

    void operator()(http_client *client = 0, boost::system::error_code ec = {}, std::vector<char>& buf = dummy())
    {
        reenter(this) {
            while ( !request_queue_.empty()) {
                current_job_ = std::move(request_queue_.back());
                request_queue_.pop_back();

                yield client->start(std::move(current_job_.request_handler), std::move(*this));
                current_job_.response_handler(client, ec, buf);
            }
        }
    }

private:
    static std::vector<char>& dummy() { static std::vector<char> x; return x; }

private:
    boost::asio::io_service& io_service_;
    std::vector<http_job_t>& request_queue_;
    boost::shared_ptr<http_client> client_;
    http_job_t current_job_;
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

    struct is_expired
    {
        template <typename Value>
        bool operator()(Value const& p) const { return p.expired(); }
    };

    void add(request_handler_t&& request_handler, response_handler_t&& response_handler)
    {
        request_queue_.emplace_back(request_handler, response_handler);

        auto ite = std::find_if(pool_.begin(), pool_.end(), is_expired());
        if (ite != pool_.end()) {
            auto client = make_http_client(io_service_, host_, port_);
            *ite = client;
            task_t task(io_service_, request_queue_, std::move(client));
            task();
        }

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
