#ifndef ASYNC_TRANSPORT_POINT_HPP
#define ASYNC_TRANSPORT_POINT_HPP

#include "boost/asio.hpp"

#include <deque>
#include <string>

namespace async_transport {

    template <typename StreamType>
    class point_iface {

    public:

        typedef StreamType stream_type;

    private:

        boost::asio::io_service         &ios_;
        boost::asio::io_service::strand &write_dispatcher_;
        std::deque<std::string>         &write_queue_;

    public:

        point_iface( boost::asio::io_service &ios )
            :ios_(ios)
        { }

        virtual ~point_iface( ) { }

    };

}


#endif // ASYNC_TRANSPORT_POINT_HPP
