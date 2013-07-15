#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/range/as_literal.hpp>
#include <network/async_http_request.hpp>
#include <network/connection_pool.hpp>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 4) {
        cerr << "Invalid arguments." << endl;
        return 1;
    }

    boost::asio::io_service io_service;

    boost::shared_ptr<network::http_client> c = network::async_http_get(io_service, argv[1], argv[2], argv[3],
        [](network::http_client *, ::boost::system::error_code ec, std::vector<char>& buf) {
            if (ec) {
                cout << ec <<endl;
                return;
            }

            string s(buf.begin(), buf.end());
            cout << s << endl;
        }
    );

    io_service.run();

    return 0;
}
