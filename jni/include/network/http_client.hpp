#pragma once

namespace network {

class http_client
{
public:
    virtual ~http_client() = 0;
};

inline
http_client::~http_client()
{
}

} // namespace network
