#include <iostream>
#include "boost/asio.hpp"

#include <queue>

#include "common/async-transport-point.hpp"

namespace ba = boost::asio;

typedef ba::ip::tcp::socket stream_type;

typedef async_transport::point_iface<stream_type> async_point_type;
typedef async_point_type::shared_type stream_sptr;

std::vector<stream_sptr> connections;

void start_accept( ba::ip::tcp::acceptor &accept );

void on_client_read( stream_sptr ptr, const char *data, size_t lenght )
{
    std::cout << "Read " << lenght << "bytes from "
              << ptr->stream( ).remote_endpoint( )
              << ": '"
              << std::string( data, lenght ) << "'\n";
}

void accept_handle( boost::system::error_code const &err,
                    stream_sptr stream,
                    ba::ip::tcp::acceptor &accept )
{
    if( !err ) {
        connections.push_back( stream );
        stream->on_read_connect( boost::bind( on_client_read, stream, _1, _2 ));
        stream->start_read( );
        start_accept( accept );
        std::cout << "new point accepted: "
                  << stream->stream( ).remote_endpoint( )
                  << std::endl;
    } else {
        std::cout << "accept error\n";
    }
}

void start_accept( ba::ip::tcp::acceptor &accept )
{
    stream_sptr new_point = async_point_type::create( accept.get_io_service( ));
    accept.async_accept(
                new_point->stream( ),
                boost::bind( accept_handle, ba::placeholders::error,
                             new_point, boost::ref( accept ) ) );
}

ba::ip::tcp::endpoint make_endpoint( const std::string &address,
                                    unsigned short port )
{
    return ba::ip::tcp::endpoint(ba::ip::address::from_string(address), port);
}


int main( ) try
{

    ba::io_service       ios;
    ba::io_service::work wrk(ios);

    ba::ip::tcp::acceptor acceptor( ios, make_endpoint("127.0.0.1", 55555) );

    start_accept( acceptor );

    while( 1 ) {
        ios.run( );
    }

    return 0;

} catch( const std::exception &ex ) {

    std::cout << "main error: " << ex.what( ) << "\n";
    return 1;

}

