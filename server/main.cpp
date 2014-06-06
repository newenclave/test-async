#include <iostream>
#include "boost/asio.hpp"

#include <queue>
#include <set>

#include "boost/thread.hpp"

#include "common/async-transport-point.hpp"

namespace ba = boost::asio;

typedef ba::ip::tcp::socket stream_type;

typedef async_transport::point_iface<stream_type> async_point_type;
typedef async_point_type::shared_type stream_sptr;

std::set<stream_sptr> connections;

void start_accept( ba::ip::tcp::acceptor &accept );

class my_transformer: public async_transport::message_transformer
{
    const  std::string key_;
    size_t counter;

public:
    my_transformer( const std::string &key )
        :key_(key)
        ,counter(0)
    { }

private:
    std::string transform( std::string &data )
    {
        std::cout << "tramsform " << data.size( ) << " bytes of data\n";
        for( size_t i=0; i<data.size( ); ++i ) {
            ++counter;
            counter %= key_.size( );
            data[i] ^= key_[ counter ];
        }
        return data;
    }
};

void on_error( stream_sptr ptr, const boost::system::error_code &err )
{
    std::cout << "Read error at "
              << ptr->stream( ).remote_endpoint( ) << ": "
              << err.message( )
              << "\n"
              ;
    ptr->close( );
    connections.erase( ptr );
    std::cout << "clients: " << connections.size( ) << "\n";
}

void write( stream_sptr ptr, std::string res )
{
    ptr->write( res );
    ptr->write( std::string("]") );
}

void on_client_read( stream_sptr ptr, const char *data, size_t lenght )
{
    std::cout << "Read " << lenght << " bytes from "
              << ptr->stream( ).remote_endpoint( ) << "\n";

    std::string res( data, data + lenght );
    std::reverse( res.begin( ), res.end( ) );

    ptr->get_io_service( ).post( boost::bind( write, ptr, res ) );
    ptr->get_io_service( ).post( boost::bind( write, ptr, res ) );

}

void accept_handle( boost::system::error_code const &err,
                    stream_sptr stream,
                    ba::ip::tcp::acceptor &accept )
{
    if( !err ) {

        connections.insert( stream );

        stream->on_read_connect( boost::bind( on_client_read, stream, _1, _2 ));
        stream->on_read_error_connect( boost::bind( on_error, stream, _1 ));
        stream->start_read( );

        //stream->set_transformer( new my_transformer( "123789" ) );

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
    stream_sptr new_point(new async_point_type(accept.get_io_service( )));
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

void run_ios( ba::io_service &ios )
{
    while( 1 ) {
        ios.run( );
    }
}

class observer_connection {
public:
    virtual ~observer_connection( ) { }
    virtual void disconnect( ) { }
};

class observer {
    virtual ~observer( ) { }
};

int main( ) try
{

    ba::io_service       ios;
    ba::io_service::work wrk(ios);

    ba::ip::tcp::acceptor acceptor( ios, make_endpoint("127.0.0.1", 55555) );

    start_accept( acceptor );

//    boost::thread t1( run_ios, boost::ref( ios ) );
//    boost::thread t2( run_ios, boost::ref( ios ) );
//    boost::thread t3( run_ios, boost::ref( ios ) );
//    boost::thread t4( run_ios, boost::ref( ios ) );

    while( 1 ) {
        ios.run( );
    }

    return 0;

} catch( const std::exception &ex ) {

    std::cout << "main error: " << ex.what( ) << "\n";
    return 1;

}

