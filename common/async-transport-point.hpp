#ifndef ASYNC_TRANSPORT_POINT_HPP
#define ASYNC_TRANSPORT_POINT_HPP

#include "boost/asio.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/bind.hpp"
#include "boost/make_shared.hpp"
#include "boost/weak_ptr.hpp"

#include <deque>
#include <string>

#include "vtrc-signal-declaration.h"

namespace async_transport {

    template <typename ST>
    class point_iface: public boost::enable_shared_from_this<point_iface<ST> > {

        typedef point_iface<ST> this_type;

    public:

        typedef ST stream_type;
        typedef boost::shared_ptr<this_type> shared_type;
        typedef boost::weak_ptr<this_type>   weak_type;

    private:

        boost::asio::io_service         &ios_;
        boost::asio::io_service::strand  write_dispatcher_;
        stream_type                      stream_;

        typedef void ( this_type::*read_impl )( );

        read_impl                        read_impl_;

        typedef std::string messate_type;

        struct queue_container {
            char         priority_;
            messate_type message_;
            queue_container( )
                :priority_(0)
            { }
        };

        typedef boost::shared_ptr<queue_container> queue_container_sptr;

        std::deque<queue_container_sptr>  write_queue_;
        std::vector<char>                 read_buffer_;

        VTRC_DECLARE_SIGNAL( on_read, void ( const char *, size_t ) );

        VTRC_DECLARE_SIGNAL( on_read_error,
                             void ( const boost::system::error_code & ) );

        VTRC_DECLARE_SIGNAL( on_write_error,
                             void ( const boost::system::error_code & ) );

    protected:

        point_iface( boost::asio::io_service &ios,
                     size_t read_block_size )
            :ios_(ios)
            ,write_dispatcher_(ios_)
            ,stream_(ios_)
            ,read_impl_(&this_type::start_read_impl_wrap)
            ,read_buffer_(read_block_size)
        { }


    public:

        virtual ~point_iface( ) { }

        static
        boost::shared_ptr<point_iface> create( boost::asio::io_service &ios )
        {
            shared_type inst( new this_type( ios, 4096) );
            return inst;
        }

    private:

        /// ================ write ================ ///
        void async_write( const char *data, size_t length, size_t total )
        {
            try {
                stream_.async_write_some(
                        boost::asio::buffer( data, length ),
                        write_dispatcher_.wrap(
                            boost::bind( &this_type::write_handler, this,
                                 boost::asio::placeholders::error,
                                 boost::asio::placeholders::bytes_transferred,
                                 length, total,
                                 this->shared_from_this( )))
                        );
            } catch( const std::exception & ) {
                ;;; /// generate error
            }
        }

        void async_write(  )
        {
            async_write( write_queue_.front( )->message_.c_str( ),
                         write_queue_.front( )->message_.size( ), 0);
        }

        void write_handler( const boost::system::error_code &error,
                            size_t const bytes,
                            size_t const length,
                            size_t       total,
                            shared_type  /*this_inst*/)
        {
            queue_container &top( *write_queue_.front( ) );

            if( !error ) {

                if( bytes < length ) {

                    total += bytes;

                    const std::string &top_mess(top.message_);
                    async_write(top_mess.c_str( ) + total,
                                top_mess.size( )  - total, total);

                } else {

                    write_queue_.pop_front( );

                    if( !write_queue_.empty( ) )
                        async_write(  );
                }
            } else {
                on_write_error_( error );
                /// generate error
            }

        }

        void write_impl( char /*priority*/, const queue_container_sptr data,
                         shared_type /*inst*/ )
        {
            bool queue_empty = write_queue_.empty( );
            write_queue_.push_back( data );

            if( queue_empty ) {
                async_write(  );
            }
        }

        /// ================ read ================ ///
        void read_handler( const boost::system::error_code &error,
                           size_t const bytes, shared_type /*weak_inst*/ )
        {
            if( !error ) {
                on_read_( &read_buffer_[0], bytes );
                start_read( );
            } else {
                /// genegate error;
                on_read_error_( error );
            }
        }

        void start_read_impl_wrap(  )
        {
            stream_.async_read_some(
                boost::asio::buffer( &read_buffer_[0], read_buffer_.size( ) ),
                    write_dispatcher_.wrap(
                        boost::bind( &this_type::read_handler, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred,
                            this->shared_from_this( )))
                );
        }

        void start_read_impl(  )
        {
            stream_.async_read_some(
                boost::asio::buffer( &read_buffer_[0], read_buffer_.size( ) ),
                        boost::bind( &this_type::read_handler, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred,
                            this->shared_from_this( ))
                );
        }

    public:

        boost::asio::io_service &get_io_service( )
        {
            return ios_;
        }

        const boost::asio::io_service &get_io_service( ) const
        {
            return ios_;
        }

        stream_type &stream( )
        {
            return stream_;
        }

        const stream_type &stream( ) const
        {
            return stream_;
        }

        void write( const std::string &data )
        {
            write( data.c_str( ), data.size( ) );
        }

        void write( const char *data, size_t length )
        {
            queue_container_sptr inst(boost::make_shared<queue_container>( ));
            inst->message_.assign( data, length );
            write_dispatcher_.post(
                        boost::bind( &this_type::write_impl, this,
                                     0, inst, this->shared_from_this( ) ) );
        }

        void start_read( )
        {
            (this->*read_impl_)( );
        }

    };

}


#endif // ASYNC_TRANSPORT_POINT_HPP
